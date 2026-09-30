#!/usr/bin/env python3
"""Extract Akai MAX49 .upd firmware into clean Intel HEX and raw flash binary."""
from __future__ import annotations
import argparse
from pathlib import Path
import re
import struct

MAGIC = b"AkaiUpdateFile"

def valid_ihex_line(line: bytes):
    if not line.startswith(b":"):
        return None
    try:
        raw = bytes.fromhex(line[1:].decode("ascii"))
    except Exception:
        return None
    if len(raw) < 5 or len(raw) != raw[0] + 5 or (sum(raw) & 0xFF):
        return None
    return raw

def locate_ihex(blob: bytes):
    for m in re.finditer(rb":02000004[0-9A-Fa-f]{6}(?:\r?\n)", blob):
        line = m.group(0).rstrip(b"\r\n")
        raw = valid_ihex_line(line)
        if raw and raw[3] == 0x04:
            return m.start()
    raise ValueError("No valid Intel HEX extended-linear-address record found")

def parse_ihex(blob: bytes, start: int):
    mem: dict[int, int] = {}
    clean_lines: list[bytes] = []
    upper = 0
    start_linear = None
    pos = start
    eof_end = None

    for line_with_end in blob[start:].splitlines(keepends=True):
        line = line_with_end.rstrip(b"\r\n")
        raw = valid_ihex_line(line)
        if raw is None:
            if eof_end is not None:
                break
            raise ValueError(f"Invalid Intel HEX record near file offset 0x{pos:X}: {line[:64]!r}")

        clean_lines.append(line)
        n = raw[0]
        addr = int.from_bytes(raw[1:3], "big")
        rectype = raw[3]
        data = raw[4:4+n]

        if rectype == 0x00:
            absolute = upper + addr
            for i, value in enumerate(data):
                mem[absolute + i] = value
        elif rectype == 0x04:
            upper = int.from_bytes(data, "big") << 16
        elif rectype == 0x05:
            start_linear = int.from_bytes(data, "big")
        elif rectype == 0x01:
            eof_end = pos + len(line_with_end)
            break

        pos += len(line_with_end)

    if not mem:
        raise ValueError("Intel HEX contained no data records")
    return mem, clean_lines, start_linear, eof_end

def contiguous_ranges(addresses):
    addresses = sorted(addresses)
    out = []
    s = p = addresses[0]
    for a in addresses[1:]:
        if a != p + 1:
            out.append((s, p))
            s = a
        p = a
    out.append((s, p))
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("upd", type=Path)
    ap.add_argument("--out-dir", type=Path, default=Path("."))
    args = ap.parse_args()

    blob = args.upd.read_bytes()
    if not blob.startswith(MAGIC):
        raise SystemExit("Not an AkaiUpdateFile container")

    args.out_dir.mkdir(parents=True, exist_ok=True)
    stem = args.upd.stem

    fmt_version = struct.unpack_from("<H", blob, len(MAGIC))[0]
    model_len = struct.unpack_from("<I", blob, len(MAGIC) + 2)[0]
    model_off = len(MAGIC) + 6
    model = blob[model_off:model_off + model_len].decode("ascii", "replace")
    ver_off = model_off + model_len
    fw_major = struct.unpack_from("<H", blob, ver_off)[0]
    fw_minor = struct.unpack_from("<H", blob, ver_off + 2)[0]

    png_sig = b"\x89PNG\r\n\x1a\n"
    png_start = blob.find(png_sig)
    if png_start >= 0:
        iend = blob.find(b"IEND", png_start)
        if iend >= 0 and iend + 8 <= len(blob):
            png_end = iend + 8
            (args.out_dir / f"{stem}_embedded.png").write_bytes(blob[png_start:png_end])

    ihex_start = locate_ihex(blob)
    mem, lines, start_linear, eof_end = parse_ihex(blob, ihex_start)

    clean_hex = args.out_dir / f"{stem}_clean.hex"
    clean_hex.write_bytes(b"\r\n".join(lines) + b"\r\n")

    lo, hi = min(mem), max(mem)
    raw = bytearray([0xFF]) * (hi - lo + 1)
    for addr, value in mem.items():
        raw[addr - lo] = value
    raw_path = args.out_dir / f"{stem}_flash_{lo:08X}.bin"
    raw_path.write_bytes(raw)

    print(f"Container: {args.upd}")
    print(f"Akai container format: {fmt_version}")
    print(f"Model: {model}")
    print(f"Firmware version fields: {fw_major}.{fw_minor:02d}")
    print(f"Intel HEX offset: 0x{ihex_start:X}")
    print(f"Start linear address: {start_linear:#010x}" if start_linear is not None else "Start linear address: none")
    print(f"Flash span: {lo:#010x} - {hi:#010x} ({hi-lo+1} bytes incl. gaps)")
    print(f"Programmed bytes: {len(mem)}")
    print("Ranges:")
    for s, e in contiguous_ranges(mem):
        print(f"  {s:#010x} - {e:#010x}  ({e-s+1} bytes)")
    print(f"Clean HEX: {clean_hex}")
    print(f"Raw BIN:   {raw_path}")

    if lo <= 0x08004000 and hi >= 0x08004008:
        off = 0x08004000 - lo
        sp, reset = struct.unpack_from("<II", raw, off)
        print(f"Vector @ 0x08004000: SP={sp:#010x} RESET={reset:#010x}")

if __name__ == "__main__":
    main()
