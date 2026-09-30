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

## First capture core

src/midi_capture.c currently provides:

- circular timestamped MIDI-event history
- copy-last-window operation
- free-play tempo estimation from Note-On inter-onset intervals
- simple 1/2/4/8/16-bar phrase-length selection

Host tests currently recover humanized phrases at 120.0 BPM and 103.75 BPM.

The tempo estimator scores candidate tempos against integer multiples of a
sixteenth-note grid using timestamp differences. Because it uses differences,
it does not need to know where beat 1 occurred before estimating tempo.

This is only the first estimator. Later work should improve:

- half/double-tempo ambiguity
- swing/triplet-heavy playing
- phrase/downbeat phase inference
- pickup notes
- confidence scoring
- preserving unquantized performance timing while fitting a loop envelope

## RAM budget

Reverse engineering of the stock v1.12 startup code shows:

    .data copy:
      0x20000000 + 0x0EA4 bytes

    .bss/ZI initialization:
      0x20000EA4 + 0x3BCC bytes

    initial application MSP:
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

mc_event_t is 8 bytes on the intended ABI, so example budgets are:

    2048 rolling events = 16 KB
    1536 stored events  = 12 KB
                         -------
                          28 KB

That is enough for a useful always-listening capture window and several
ordinary MIDI clips. Denser clip storage can later use delta timestamps or a
packed event representation.

## UI direction

A clip workflow fits the MAX49 panel better than deep step editing:

- pads select/launch clips
- REC records/overdubs the selected clip
- a dedicated Capture gesture recalls recent playing
- PLAY/STOP remain transport
- LCD shows clip number, bar length, tempo and state
- faders remain performance controls / synth parameters

Example display:

    CLIP 03
    08 BAR   119.8

or after free-play capture:

    CAPTURED
    04 BAR   103.8

The old blue LCD is an advantage here: the workflow only needs terse state,
tempo and clip information, not a piano roll.
