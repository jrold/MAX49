# Akai MAX49 v1.12 reverse-engineering notes

## Container

- Magic: AkaiUpdateFile
- Model: MAX49
- v1.12 payload is ordinary Intel HEX, not encrypted.
- Intel HEX begins inside the .upd around file offset 0x8FB0.

## Flash map

- vector table: 0x08004000
- initial SP: 0x20000400
- reset vector: 0x080041AB
- main programmed region: 0x08004000-0x0802700B
- metadata: 0x08027800-0x0802780F and 0x08027FFC-0x08027FFF
- inferred resident bootloader: 0x08000000-0x08003FFF
- persistent program/config area begins at 0x08028000
- firmware validates writable flash against one-past-end 0x08040000, strongly indicating 256 KB flash

## Processor

ARM Thumb/Thumb-2 firmware directly uses the STM32F1 peripheral map. The interrupt layout matches STM32F103 high-density parts.

Strong conclusion: STM32F103 high-density Cortex-M3, likely 256 KB xC-class. Exact package suffix still needs the physical chip marking or schematic.

## Useful interrupt handlers

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

Likely functions in the 0x080100A0-0x080101DC region:

- Channel Pressure
- Control Change
- Note Off
- Note On
- Pitch Bend
- Program Change

Routing wrappers near 0x080140DC / 0x0801411C feed the existing USB/DIN routing logic.

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

## Next RE targets

1. map a safe button/menu trigger path
2. verify SysEx sender ABI and DIN path
3. reproduce firmware integrity/checksum behavior
4. recover updater/bootloader wire protocol
5. trace FPGA traffic
6. map bidirectional fader/LED state updates