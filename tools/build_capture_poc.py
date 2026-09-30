#!/usr/bin/env python3
"""Build MAX49 v1.12 NOOP and Capture POC .upd files from a user-supplied Akai update."""
from __future__ import annotations
import argparse, hashlib, shutil, struct, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"
STOCK_SHA256 = "6c9ae99ca2d3ee4b69e25ca30e725fcde2d4625d08114223bc07b56843e7fb0d"

HOOK_DOWN_ADDR = 0x08006AB4
HOOK_UP_ADDR = 0x08006B0C
PAYLOAD_ADDR = 0x0802700C
EXPECTED_DOWN = bytes.fromhex("0b f0 7a fa")
EXPECTED_UP = bytes.fromhex("0b f0 0a fa")
NOOP_MARKER = b"MAX49_NOOP_TEST_V1\x00"

def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()

def tool(name: str) -> str:
    p = shutil.which(name)
    if not p:
        raise SystemExit(f"missing build tool: {name}")
    return p

def run(cmd: list[str]) -> None:
    print("+", " ".join(cmd))
    subprocess.run(cmd, cwd=ROOT, check=True)

def parse_line(line: bytes) -> bytearray:
    if not line.startswith(b":"):
        raise ValueError("not Intel HEX")
    raw = bytearray.fromhex(line[1:].decode("ascii"))
    if len(raw) != raw[0] + 5 or (sum(raw) & 0xFF):
        raise ValueError("bad Intel HEX checksum/length")
    return raw

def line_from_raw(raw: bytearray) -> bytes:
    raw[-1] = (-sum(raw[:-1])) & 0xFF
    return b":" + bytes(raw).hex().upper().encode("ascii")

def data_record(addr16: int, data: bytes) -> bytes:
    raw = bytearray([len(data), (addr16 >> 8) & 0xFF, addr16 & 0xFF, 0])
    raw += data
    raw += b"\x00"
    return line_from_raw(raw)

def compile_payload() -> tuple[bytes, bytes, bytes]:
    BUILD.mkdir(exist_ok=True)
    clang, lld, objcopy = tool("clang"), tool("ld.lld"), tool("llvm-objcopy")
    run([clang, "--target=arm-none-eabi", "-mcpu=cortex-m3", "-mthumb", "-Oz",
         "-ffreestanding", "-fno-builtin", "-fno-unwind-tables",
         "-fno-asynchronous-unwind-tables", "-fdata-sections", "-ffunction-sections",
         "-c", "firmware/capture_poc.c", "-o", "build/capture_poc.o"])
    run([clang, "--target=arm-none-eabi", "-mcpu=cortex-m3", "-mthumb",
         "-c", "firmware/capture_hooks.S", "-o", "build/capture_hooks.o"])
    run([lld, "-T", "firmware/capture_poc.ld", "--gc-sections",
         "build/capture_hooks.o", "build/capture_poc.o", "-o", "build/capture_poc.elf"])
    outs = []
    for sec, name in [(".hook_down","hook_down.bin"),(".hook_up","hook_up.bin"),(".payload","payload.bin")]:
        path = BUILD / name
        run([objcopy, "-O", "binary", f"--only-section={sec}", "build/capture_poc.elf", f"build/{name}"])
        outs.append(path.read_bytes())
    down, up, payload = outs
    if len(down) != 4 or len(up) != 4:
        raise RuntimeError("hook size changed")
    if len(payload) > 0x7F4:
        raise RuntimeError("payload no longer fits first flash cave")
    return down, up, payload

def patch_upd(stock_blob: bytes, hook_down: bytes, hook_up: bytes, payload: bytes) -> bytes:
    blob = bytearray(stock_blob)
    start = blob.find(b":020000040800")
    eofrec = blob.find(b":00000001FF", start)
    if start < 0 or eofrec < 0:
        raise ValueError("embedded Intel HEX not found")
    end = eofrec + len(b":00000001FF")
    if blob[end:end+2] == b"\r\n": end += 2
    elif blob[end:end+1] == b"\n": end += 1
    else: raise ValueError("Intel HEX EOF newline missing")

    old_ihex = bytes(blob[start:end])
    lines = old_ihex.splitlines()
    upper = 0
    found_down = found_up = inserted = False
    out: list[bytes] = []

    for line in lines:
        raw = parse_line(line)
        n = raw[0]
        addr = (raw[1] << 8) | raw[2]
        typ = raw[3]
        data = bytearray(raw[4:4+n])

        if typ == 4:
            upper = int.from_bytes(data, "big") << 16

        if typ == 0:
            absolute = upper + addr
            for pa, patch, expected, which in [
                (HOOK_DOWN_ADDR, hook_down, EXPECTED_DOWN, "down"),
                (HOOK_UP_ADDR, hook_up, EXPECTED_UP, "up"),
            ]:
                a0, a1 = max(absolute, pa), min(absolute+n, pa+len(patch))
                if a0 < a1:
                    ro, po = a0-absolute, a0-pa
                    old = bytes(data[ro:ro+(a1-a0)])
                    exp = expected[po:po+(a1-a0)]
                    if old != exp:
                        raise ValueError(f"{which} hook stock bytes mismatch")
                    data[ro:ro+(a1-a0)] = patch[po:po+(a1-a0)]
                    if which == "down": found_down = True
                    else: found_up = True
            raw[4:4+n] = data
            line = line_from_raw(raw)

            if not inserted and absolute <= 0x0802700B < absolute+n:
                out.append(line)
                for pos in range(0, len(payload), 16):
                    a = PAYLOAD_ADDR + pos
                    out.append(data_record(a & 0xFFFF, payload[pos:pos+16]))
                inserted = True
                continue
        out.append(line)

    if not (found_down and found_up and inserted):
        raise RuntimeError("failed to place all patches")

    new_ihex = b"\r\n".join(out) + b"\r\n"
    for line in new_ihex.splitlines(): parse_line(line)

    # Long updater record layout immediately before Intel HEX:
    # u32 type=1, u32 sysex_len, then F0 47 00 7D 70, IHEX, F7, u32 delay.
    old_len = struct.unpack_from("<I", blob, start - 9)[0]
    if old_len != len(old_ihex) + 6:
        raise ValueError("unexpected Akai updater record layout")
    struct.pack_into("<I", blob, start - 9, len(new_ihex) + 6)
    return bytes(blob[:start]) + new_ihex + bytes(blob[end:])

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("stock_upd", type=Path, help="original Akai MAX49_v1_12.upd")
    ap.add_argument("--out-dir", type=Path, default=BUILD)
    args = ap.parse_args()

    stock = args.stock_upd.read_bytes()
    if sha256(stock) != STOCK_SHA256:
        raise SystemExit("refusing: stock .upd fingerprint does not match supported Akai v1.12 file")

    down, up, payload = compile_payload()
    args.out_dir.mkdir(parents=True, exist_ok=True)

    noop = patch_upd(stock, EXPECTED_DOWN, EXPECTED_UP, NOOP_MARKER)
    poc = patch_upd(stock, down, up, payload)
    np = args.out_dir / "MAX49_v1_12_NOOP_TEST.upd"
    pp = args.out_dir / "MAX49_v1_12_CAPTURE_POC.upd"
    np.write_bytes(noop); pp.write_bytes(poc)

    print(f"NOOP: {np}  sha256={sha256(noop)}")
    print(f"POC:  {pp}  sha256={sha256(poc)}")
    print(f"payload={len(payload)} bytes")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
