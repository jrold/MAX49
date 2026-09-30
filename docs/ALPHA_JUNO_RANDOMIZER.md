# Alpha Juno randomizer on MAX49

## Bottom line

Yes: the MAX49 can easily do the same category-aware Alpha Juno patch generation used by lcxl3-alpha-juno.

The job is tiny for the STM32F103 Cortex-M3. Generating 36 bounded pseudo-random values is effectively free. The slow part is transmitting the resulting MIDI bytes at DIN MIDI speed.

## Roland live parameter message

Alpha Juno individual-parameter editing uses a 10-byte message:

    F0 41 36 0n 23 20 01 pp vv F7

where n is the MIDI channel nibble, pp is the parameter number and vv is the value.

MAX49 firmware already contains a generic arbitrary-length MIDI/SysEx transmit path, so the custom code only needs to construct the message and call the existing sender.

## Randomizer behavior to preserve from lcxl3-alpha-juno v3.4

Commands:

- 0: INIT
- 1: RAND ALL
- 2: DARK
- 3: BRIGHT
- 8: BASS
- 9: LEAD
- 10: PAD
- 11: PLUCK
- 12: KICK
- 13: SNARE
- 14: HAT
- 15: FX

Each category contains min/max ranges for the 36 Alpha Juno parameters.

Policy:

- Preserve parameter 9 / HPF CUTOFF.
- Preserve parameter 22 / VCA LEVEL.
- BASS and LEAD request the old-ROM one-note Chord Memory mono workaround.
- Other categories restore POLY.
- BASS, LEAD and PAD force parameter 11 / DCO LFO DEPTH to 0 for stable pitch.

## MAX49 integration

The reusable generator should remain independent of Akai firmware internals. MAX49-specific glue then provides:

1. an entropy/seed value from a timer or event timestamp
2. a callback that sends one Alpha Juno parameter SysEx message
3. a callback that sends the MONO/POLY channel-mode message
4. a trigger source from a MAX49 button/menu/program action

A first proof-of-concept does not need a new MAX49 program schema. The safest route is to redirect an existing mode/action only inside a dedicated test program, prove DIN SysEx and randomization on hardware, then add a clean AJUNO/SYSEX mode to the UI.

## Possible UI

One useful end-state:

- four banks of eight touch faders = 32 direct synth parameters
- switches under faders = discrete/toggle parameters or generator actions
- dedicated buttons = RAND ALL, BASS, LEAD, PAD, PLUCK, drums, FX
- LED ladders = current parameter values

Incoming Alpha Juno SysEx can later be parsed to update logical fader positions and LED ladders, turning the MAX49 into a PG-300-like bidirectional programmer.

## Current blockers before flashing custom code

- identify a low-risk hook for the chosen trigger/button
- verify the exact internal SysEx-send ABI on hardware
- understand/reproduce the MAX49 application integrity/checksum requirement
- establish a tested recovery/restore path using the resident bootloader

Until those are done, the randomizer core can be built and tested off-device without risking the keyboard.