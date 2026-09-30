#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../src/midi_capture.h"

static void add(mc_event_t *e, size_t *n, int t, int note, int vel)
{
    e[*n] = (mc_event_t){(uint32_t)t, 0x90, (uint8_t)note, (uint8_t)vel, 0};
    (*n)++;
}

int main(void)
{
    mc_event_t ev[256];
    size_t n = 0;

    const int base[] = {
        0, 248, 503, 1007, 1253, 1501, 2004, 2251, 2507, 3002, 3504,
        4008, 4250, 4504, 5000, 5507, 6003, 6251, 6501, 7004, 7501
    };
    for (size_t i = 0; i < sizeof(base) / sizeof(base[0]); i++)
        add(ev, &n, base[i] + ((int)(i % 3) - 1) * 5, 60 + (int)(i % 7), 100);

    mc_tempo_result_t r;
    assert(mc_estimate_tempo(ev, n, 70, 180, &r));
    printf("tempo %.2f score %.6f onsets %u\n", r.bpm, r.score, r.onset_count);
    assert(fabsf(r.bpm - 120.0f) < 1.0f);
    assert(mc_choose_bar_count(7980, r.bpm, 16) == 4);

    mc_event_t storage[8], out[8];
    mc_ring_t ring;
    mc_ring_init(&ring, storage, 8);
    for (int i = 0; i < 12; i++)
        mc_ring_push(&ring, 1000u * i, 0x90, 60, 100, 0);

    size_t got = mc_ring_copy_recent(&ring, out, 8, 11000, 4000);
    assert(got == 5);
    assert(out[0].time_ms == 7000 && out[4].time_ms == 11000);

    n = 0;
    const float q = 60000.0f / 103.75f;
    const float units2[] = {
        0, .5f, 1, 1.5f, 2.5f, 3, 4, 4.5f, 5.5f, 6,
        7, 8, 8.5f, 9, 10, 11, 12, 13.5f, 14, 15
    };
    for (size_t i = 0; i < sizeof(units2) / sizeof(units2[0]); i++) {
        int jitter = (int)(i % 5) - 2;
        add(ev, &n, (int)(units2[i] * q + jitter * 4), 64 + (int)(i % 5), 95);
    }

    assert(mc_estimate_tempo(ev, n, 70, 180, &r));
    printf("tempo2 %.2f score %.6f onsets %u\n", r.bpm, r.score, r.onset_count);
    assert(fabsf(r.bpm - 103.75f) < 1.0f);

    puts("midi_capture: ok");
    return 0;
}
