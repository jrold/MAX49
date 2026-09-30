# Akai MAX49 v1.12 reverse-engineering notes

## Container

- Magic: AkaiUpdateFile
- Model: MAX49
- v1.12 payload is ordinary Intel HEX, not encrypted.
- Intel HEX begins inside the .upd around file offset 0x8FB0.

## Flash map

- vector table: 0x08004000
- vector-table SP word: 0x20000400
- reset vector: 0x080041AB
- reset code explicitly sets MSP to 0x20004A70 before entering the application
- main programmed region: 0x08004000-0x0802700B
- metadata: 0x08027800-0x0802780F and 0x08027FFC-0x08027FFF
- inferred resident bootloader: 0x08000000-0x08003FFF
- persistent program/config area begins at 0x08028000
- firmware validates writable flash against one-past-end 0x08040000, strongly indicating 256 KB flash

## Processor

ARM Thumb/Thumb-2 firmware directly uses the STM32F1 peripheral map. The interrupt layout matches STM32F103 high-density parts.

Strong conclusion: STM32F103 high-density Cortex-M3, likely a 256 KB xC-class device. Exact package suffix still needs the physical chip marking or schematic.

## SRAM / startup map

The startup table exposes the stock application's static RAM footprint:

- .data: copy 0x0EA4 bytes to 0x20000000
- .bss/ZI: zero 0x3BCC bytes beginning at 0x20000EA4
- resulting static-RAM end: 0x20004A70
- reset code sets MSP to 0x20004A70

A PC-relative literal-reference scan of the application found genuine stock global-RAM references only up to approximately 0x20004650. No genuine stock literal references were found in 0x20005000-0x2000BFFF.

If the physical MCU is the expected 48 KB-SRAM STM32F103 xC-class part, 0x20005000-0x2000BFFF is a very attractive 28 KB candidate region for custom capture/clip state.

Do not treat that upper region as proven-safe on hardware yet. Confirm the exact MCU marking and/or perform a non-destructive runtime RAM test first.

## SysTick / timebase

SysTick handler: 0x08013AEC.

The handler calls 0x0801AD30, which increments a 16-bit counter at:

- 0x20000C12

Accessor:

- 0x08013F94 returns the current 16-bit tick count

Initializer around 0x08013FA0 programs:

- SysTick LOAD = 0x12BF (4799)
- SysTick CTRL = 7 after enable (processor clock, interrupt enabled, counter enabled)

At a 72 MHz Cortex-M3 clock this is a 15 kHz tick, about 66.67 microseconds per tick. The stock 16-bit counter therefore wraps every ~4.37 seconds.

For long capture timestamps, custom firmware should extend this to a 32-bit timebase rather than relying directly on the stock 16-bit value.

## Useful interrupt handlers

- SysTick: 0x08013AEC
- USB HP / CAN TX: 0x0801430D
- USB LP / CAN RX0: 0x080143BD
- SPI1: 0x08011A89
- SPI2: 0x08011A9D
- SPI3: 0x08011AB1
- USART1: 0x08014303
- USB Wakeup: 0x0801430B

## Native MAX49 SysEx

A signature checker around 0x0800E97C recognizes F0 47 xx 7D ... .

The receive dispatcher around 0x0800DD20 reads a 14-bit payload length from bytes 5/6 and dispatches on command byte 4. Observed commands include 00-08, 10, 12, 14, 16, 20, 21, 22, 30, 31, 32, 72, 76, 78, 79, 7A and 7B.

0x080149CC is a central variable-length MIDI byte-stream to USB-MIDI packetizer/transmitter and is a prime custom-SysEx call target.

## Standard MIDI output helpers

The short helpers at 0x080100A0-0x080101DC are now mapped with high confidence:

- 0x080100A0: Channel Pressure
- 0x080100D4: Control Change
- 0x08010118: Note Off
- 0x08010148: Note On
- 0x08010178: Pitch Bend
- 0x080101AC: Polyphonic Aftertouch
- 0x080101DC: Program Change

They feed the existing low-level MIDI output/routing path around 0x08012110.

Useful routing wrappers:

- 0x080141AA: Note Off routing wrapper
- 0x080141F2: Note On routing wrapper

A second generic local/output Note-On route exists at 0x0800FDB4. It accepts note and velocity, then sends through DIN and/or USB according to the MAX49 routing state. It has many callers from different panel/performance subsystems, so it is a promising capture hook but is not yet proven to represent only raw keybed input. Capturing here would likely capture post-processing/generated notes as well unless custom playback is explicitly suppressed.

## Firmware checksum / integrity

0x080080CC computes a simple bytewise XOR over:

- 0x08004000 through 0x08027FFF

The only currently identified consumer is around 0x0801C9C8, where the value is masked to 7 bits and included in an Akai SysEx response.

This does not look like a cryptographic signature or secure-boot check. It currently appears to be an informational/application checksum. Modified firmware will change the reported value; bootloader behavior still needs to be verified before assuming there is no separate update-time validation.

## Touch faders

Display/type table near 0x0801DE90:

0 MIDI CC
1 Aft
2 INC/DEC1
3 INC/DEC2
4 Mackie
5 HUI

Important regions:

- 0x080167EC: central absolute/controller fader output dispatcher
- 0x08018908: relative / INC-DEC / Mackie / HUI handling
- 0x08018710 region: fader normalization/state

High-confidence per-fader config keys:

- 0x34C: fader Type
- 0x38C: CC number in MIDI-CC mode
- 0x44C: MIDI-to-DIN routing boolean

## FPGA / internal project name

The image contains STM32vv.vv FPGAvv.vv and a metadata string AD37_Richard at 0x08027800. The FPGA/companion-device role is still unknown. SPI1/2/3 all have real IRQ handlers and are good tracing targets.

## Alpha Juno feasibility

Alpha Juno live parameter SysEx is only 10 bytes. The MAX49 already has the fader event pipeline, MIDI routing and arbitrary SysEx output. Direct editing and category-aware randomization are therefore small firmware additions.

## Realtime clip/capture feasibility

The current strongest architecture is:

1. hook a suitable local/post-performance Note-On/Note-Off path
2. timestamp events with an extended form of the existing 15 kHz timebase
3. store them in a circular buffer in candidate upper SRAM
4. on Capture, analyze recent Note-On timing and infer tempo/phrase length
5. copy the chosen recent history into a clip
6. play clips through the existing MIDI routing helpers with a capture-suppress flag to avoid feedback

The hardware-independent capture/tempo core lives in src/midi_capture.c.

## Next RE targets

1. identify whether 0x0800FDB4 is an acceptable musical-event capture point or locate the raw keybed/pad event source upstream
2. map a low-risk physical Capture button/pad hook
3. map the four-line LCD framebuffer/render path for a native clip page
4. verify upper SRAM on real hardware
5. recover updater/bootloader wire protocol and restore path
6. trace FPGA traffic
7. map bidirectional fader/LED state updates


## Raw 49-key keybed path

The physical keyboard path is now mapped much more strongly.

Polling/scanner function around 0x0800E9FC:

- reads the raw keybed event bytes
- bit 7 distinguishes the two key transitions
- 0x08008810 maps the raw scan code to a 0..48 physical-key index
- 0x08006168 converts the raw strike measurement to velocity 1..127

The scanner then calls:

- 0x0800E9C8 for the transition that carries velocity (KEY DOWN / Note On path)
- 0x0800E9AC for the transition without velocity (KEY UP / Note Off path)

Both explicitly reject key indices >= 49.

Those functions enqueue event types 2 and 3 through 0x0801B224 / 0x08015484. The wrapper packs the physical key index and velocity into the queue payload.

The central dispatcher at 0x0800D90C consumes them:

- type 2 -> 0x08006A90(key_index, velocity)
- type 3 -> 0x08006AD0(key_index)

Each of those has exactly one caller: the central dispatcher.

### Best capture hook sites

The cleanest current candidates are the two stock send calls inside those physical-key handlers:

- 0x08006AB4: BL 0x08011FAC  (physical KEY DOWN / Note On processing)
- 0x08006B0C: BL 0x08011F24  (physical KEY UP / Note Off processing)

Hooking those individual BL instructions is preferable to hooking the global MIDI transmitter because it captures only human keybed performance, not notes generated later by arp/sequencer/pads.

### Stock per-key sent-note table

The stock firmware already maintains a 128 x 2-byte note-state table at:

    0x20000FC0

Relevant helpers:

- 0x0801CC78 writes the second byte for a key
- 0x08019270 reads the second byte for a key
- the Note On path stores the mapped/sent MIDI note there
- the Note Off path reads it back so release uses the same pitch even if octave/transpose state changed while the key was held

The Note On path computes the current mapped note through 0x080087D8 before sending.

This gives Capture an excellent semantic point:

1. call the original stock physical-key Note On routine
2. read the actual mapped/sent note from the stock key-state table
3. append Note On + velocity + custom 15 kHz timestamp to capture history

For Note Off:

1. read the mapped/sent note from the stock key-state table
2. append Note Off + timestamp
3. call the original stock Note Off routine

This preserves what the player actually heard while excluding generated arp/sequence notes.


## 2026-09-30: RAM / clip-capture pass

Startup analysis now gives an exact linker/runtime RAM initialization boundary. The image copies 0xEA4 bytes from flash 0x08026CEC to SRAM 0x20000000 and zeroes 0x3BCC bytes beginning at 0x20000EA4. The initialized/zeroed span therefore ends at 0x20004A70 (19,056 bytes total). Startup also loads MSP=0x20004A70. This is the first hard RAM-footprint number from the binary. It does not yet prove the RAM above 0x20004A70 is unused at runtime; exact MCU SRAM capacity and heap/alternate-stack behavior still need verification.

A realtime MIDI clip/Capture design is documented in docs/CLIP_CAPTURE.md. The initial proof should be RAM-only and capture Note On/Off before attempting saved clips.
