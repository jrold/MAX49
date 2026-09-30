# Realtime clip capture

The MAX49 custom-firmware direction is a realtime MIDI clip recorder rather
than a deeper traditional step sequencer.

## Intended workflow

The device continuously timestamps incoming/local MIDI performance events into
a circular RAM buffer. The performer does not have to arm recording first.

After playing something useful:

    CAPTURE
      -> analyze recent Note-On timing
      -> estimate tempo
      -> choose/confirm 1, 2, 4, 8 or 16 bars
      -> copy the selected history into a clip
      -> start transport/loop playback

When transport is already running, Capture is simpler: the firmware already
knows tempo and musical phase, so it can grab exactly the previous N bars.

## Native MAX49 timebase

The v1.12 application already configures SysTick with LOAD=0x12BF and increments
a 16-bit counter at 0x20000C12 from the SysTick ISR.

With the expected 72 MHz core clock and processor-clock SysTick source this is:

    15,000 ticks/second
    66.67 microseconds/tick

The stock counter wraps after about 4.37 seconds, so the custom firmware needs
to extend it to 32 bits for capture. The capture engine uses these native 15 kHz
units directly; no floating point or millisecond quantization is required.

## Capture core

src/midi_capture.c currently provides:

- circular timestamped MIDI-event history
- copy-last-window operation
- free-play tempo estimation from Note-On inter-onset intervals
- 1/2/4/8/16-bar phrase-length selection
- 24-bit/7-byte compact MIDI event packing

The tempo estimator is integer/fixed-point and scans quarter-BPM candidates.
Host tests currently recover humanized phrases at exactly:

    120.00 BPM
    103.75 BPM

The estimator scores candidate tempos against integer multiples of a
sixteenth-note grid using timestamp differences. Because it uses differences,
it does not need to know where beat 1 occurred before estimating tempo.

A Cortex-M3 -Oz build of the capture core is currently about 835 bytes of text
before MAX49-specific glue, so the algorithm is small enough to fit comfortably
inside the known application code caves.

This is only the first estimator. Later work should improve:

- half/double-tempo ambiguity
- swing/triplet-heavy playing
- phrase/downbeat phase inference
- pickup notes
- confidence scoring
- preserving unquantized performance timing while fitting a loop envelope

## Compact events

The compact representation is:

    bytes 0..2   absolute 24-bit capture tick
    byte  3      MIDI status
    byte  4      data 1
    byte  5      data 2
    byte  6      flags

At 15 kHz, a 24-bit timestamp spans about 18.6 minutes. That is far beyond a
1/2/4/8/16-bar capture window even at very slow tempos.

A raw 0x7000-byte upper-RAM arena divided into seven-byte events is exactly
4096 event slots. Real firmware will reserve part of that arena for clip
metadata, indices, state, and the extended clock, so usable capacity will be
somewhat lower.

## RAM budget

Reverse engineering of the stock v1.12 startup code shows:

    .data copy:
      0x20000000 + 0x0EA4 bytes

    .bss/ZI initialization:
      0x20000EA4 + 0x3BCC bytes

    reset sets MSP:
      0x20004A70

Actual PC-relative global RAM references found in the application top out near
0x20004650. The stock image does not currently show genuine literal references
into 0x20005000-0x2000BFFF.

If the MCU is the expected high-density STM32F103 xC-class 48 KB SRAM part,
that leaves a very attractive candidate region:

    0x20005000-0x2000BFFF = 28 KB

This is not yet declared safe for patched firmware. Before using it on hardware
we should verify the exact MCU marking and/or perform a non-destructive runtime
RAM test.

## MIDI hook direction

0x0800FDB4 is a generic MAX49 note-output route. It receives note/velocity and
routes through DIN and/or USB according to the current device routing state.

It has many callers from different performance/control subsystems, so it is a
useful possible first capture hook but it is not yet proven to mean only raw
keybed input. Hooking there would likely make Capture capable of remembering
post-processing/generated material too. Custom clip playback would use a
capture-suppress flag so playback cannot feed itself back into the history.

The cleaner long-term option is to locate the raw keybed/pad event source
upstream and choose explicitly whether Capture records raw playing or
post-arp/post-sequencer output.

## Transport

Existing firmware already has explicit MIDI transport paths:

    0x0801021C  MIDI Start (FA)
    0x08010226  MIDI Stop  (FC)
    0x08010230  MIDI Clock (F8)

and higher-level routing/state functions around:

    0x0801221C  start path
    0x08012258  stop path

This gives the clip system existing transport infrastructure to reuse rather
than replacing the MAX49 MIDI stack.

## UI direction

A clip workflow fits the MAX49 panel better than deep step editing:

- pads select/launch clips
- REC records/overdubs the selected clip
- a dedicated Capture gesture recalls recent playing
- PLAY/STOP remain transport
- LCD shows clip number, bar length, tempo and state
- faders remain performance controls / synth parameters

Example display:

    CLIP 03   PLAY
    08 BAR   119.8
    LEN 00:16.03
    CAPTURE READY

or after free-play capture:

    CLIP 03   LOOP
    04 BAR   103.8
    UNQUANTIZED
    CAPTURED

The old blue four-line LCD is an advantage here: the workflow only needs terse
state, tempo and clip information, not a piano roll.

## Next integration work

1. extend the stock 16-bit SysTick count to a custom 32-bit capture clock
2. identify the preferred musical input hook (raw keybed vs post-processing)
3. identify a low-risk Capture button/gesture
4. map a native LCD render/page hook
5. verify upper SRAM on physical hardware
6. implement clip playback scheduling through the stock Note On/Off routes
7. implement overdub and one-level undo
