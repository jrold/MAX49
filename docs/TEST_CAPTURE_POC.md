# Capture POC hardware test

This is the first **hardware proof**, not the finished clip system.

## Test behavior

The Capture POC temporarily reserves the two end keys:

- lowest physical key: clear buffer and begin capture
- highest physical key: replay the captured buffer once

The 47 keys between them behave normally and are captured as Note On/Off events with original velocity, MIDI channel and timing.

The capture buffer holds 1360 events in upper SRAM. The first event replays immediately. Individual gaps above roughly 4.37 seconds are not represented correctly in this first build.

There is no looping, tempo inference, LCD feedback, clip launching or persistence yet.

## Why there are two test images

Always test the NOOP image first.

The NOOP image adds only the marker `MAX49_NOOP_TEST_V1` to the unused flash cave beginning at 0x0802700C. It does not patch control flow. This tests whether the stock Akai updater accepts a modified v1.12 application and whether the MAX49 boots normally afterward.

Only after NOOP works should the Capture POC be flashed.

## Flash procedure

1. Keep the original Akai `MAX49_v1_12.upd` ready for recovery.
2. Install/run the official MIDI Updater v1.05 supplied in Akai's v1.12 package.
3. Connect MAX49 directly by USB and power it normally.
4. Drag `MAX49_v1_12_NOOP_TEST.upd` into MIDI Updater.
5. Do not disconnect USB/power during the update.
6. Restart when the MAX49/updater instructs you.
7. Verify normal LCD, Program mode, key playing and USB MIDI enumeration.
8. If NOOP is completely normal, repeat the update using `MAX49_v1_12_CAPTURE_POC.upd`.
9. Restart.
10. Press the lowest key once to clear/start capture. It intentionally sends no note.
11. Play a short phrase using the 47 middle keys.
12. Release the notes.
13. Press the highest key once. It intentionally sends no note.
14. The phrase should replay once. DIN MIDI is the preferred first test destination.

Press the lowest key again to start a new capture.

## Recovery

If the Capture POC boots but key behavior/capture is wrong:

1. power-cycle MAX49
2. do not press keyboard keys
3. open MIDI Updater v1.05
4. flash the original stock `MAX49_v1_12.upd`
5. restart and verify stock operation

If the MAX49 stops enumerating over USB after a power cycle, stop. A hardware-button bootloader/recovery entry has not yet been proven; do not guess button combinations.

## Known generated fingerprints

From the exact stock Akai v1.12 file used during RE:

- stock SHA-256: `6c9ae99ca2d3ee4b69e25ca30e725fcde2d4625d08114223bc07b56843e7fb0d`
- NOOP SHA-256: `213ea03ec1c16900b5a3387070a5f837deb0e99f140a62b214ab3243b1b09fda`
- Capture POC SHA-256: `b246ebd2d474d9df2c239d9e25102ac2b16f81f4c04ae8e312fd535d342f2557`

The build tool intentionally refuses any stock file whose fingerprint differs.
