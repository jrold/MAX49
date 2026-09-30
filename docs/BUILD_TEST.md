# Host test

Compile the hardware-independent Alpha Juno randomizer on a normal machine:

    cc -std=c11 -Wall -Wextra -pedantic src/alpha_juno_randomizer.c tests/test_alpha_juno_randomizer.c -o /tmp/test-aj
    /tmp/test-aj

Expected output:

    alpha_juno_randomizer: ok

This test checks the exact Roland 10-byte live-parameter message and verifies that a BASS generation emits 34 parameter updates plus one mono-mode callback.