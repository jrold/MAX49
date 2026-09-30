# MAX49

Reverse-engineering and custom-firmware work for the Akai MAX49.

## Current hardware/firmware findings

- Stock firmware examined: Akai MAX49 v1.12.
- The .upd container is not encrypted; it contains Intel HEX.
- Main CPU: STM32F103 high-density Cortex-M3, very likely a 256 KB xC-class device.
- Application flash: approximately 0x08004000-0x08027FFF.
- 0x08000000-0x08003FFF appears to be a resident bootloader.
- 0x08028000-0x0803FFFF is used as persistent program/config storage.
- Existing generic variable-length MIDI/SysEx transmitter: 0x080149CC.
- Central touch-fader dispatcher: approximately 0x080167EC.
- Native Akai SysEx parser/dispatcher: approximately 0x0800DD20.
- Firmware contains STM32 and FPGA version reporting; the companion FPGA role is still being mapped.

## Why this is interesting

The MAX49 has plenty of compute for substantial standalone MIDI firmware. A 72 MHz Cortex-M3 with 48 KB SRAM is far beyond what is required for patch randomization, chord generation, MIDI transformations, arpeggiation, sequencing, control-rate LFOs, NRPN/SysEx translation, and synth-specific editor profiles.

The first custom target is a Roland Alpha Juno profile:

- touch faders -> Alpha Juno live-parameter SysEx
- category-aware patch randomization
- Bass / Lead / Pad / Pluck / Kick / Snare / Hat / FX generators
- optional bidirectional SysEx receive and LED-ladder feedback later

The randomizer behavior is being ported from the proven lcxl3-alpha-juno project. CPU load is negligible; DIN MIDI bandwidth is the meaningful limit.

## Repository policy

No Akai proprietary firmware image is distributed here. Tools operate on firmware supplied by the device owner.

## Layout

- docs/RE_NOTES.md - reverse-engineering notes and addresses
- docs/ALPHA_JUNO_RANDOMIZER.md - MAX49 Alpha Juno/randomizer design
- tools/extract_max49.py - extracts Akai .upd into clean Intel HEX/raw flash
- src/alpha_juno_randomizer.[ch] - hardware-independent randomizer core

## Status

Reverse engineering is active. The Alpha Juno randomizer core can be developed independently now, but a flashable MAX49 patch should not be considered ready until the firmware integrity/checksum path and safe patch/restore workflow are fully verified.