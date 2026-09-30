#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../src/midi_capture.h"

static uint32_t ms_to_ticks(int ms)
{
    return (uint32_t)ms * (MC_TICK_HZ / 1000u);
}

static void add(mc_event_t *e, size_t *n, int ms, int note, int vel)
{
    e[*n] = (mc_event_t){ms_to_ticks(ms), 0x90, (uint8_t)note, (uint8_t)vel, 0};
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
    printf("tempo %u.%02u score %u onsets %u\n",
           mc_bpm_x100(r.bpm_q4) / 100, mc_bpm_x100(r.bpm_q4) % 100,
           r.score, r.onset_count);
    assert(r.bpm_q4 == 480);
    assert(mc_choose_bar_count(ms_to_ticks(7980), r.bpm_q4, 16) == 4);

    mc_event_t storage[8], out[8];
    mc_ring_t ring;
    mc_ring_init(&ring, storage, 8);
    for (int i = 0; i < 12; i++)
        mc_ring_push(&ring, ms_to_ticks(1000 * i), 0x90, 60, 100, 0);

    size_t got = mc_ring_copy_recent(&ring, out, 8, ms_to_ticks(11000), ms_to_ticks(4000));
    assert(got == 5);
    assert(out[0].time_ticks == ms_to_ticks(7000));
    assert(out[4].time_ticks == ms_to_ticks(11000));

    n = 0;
    const double q = 60000.0 / 103.75;
    const double units[] = {
        0, .5, 1, 1.5, 2.5, 3, 4, 4.5, 5.5, 6,
        7, 8, 8.5, 9, 10, 11, 12, 13.5, 14, 15
    };
    for (size_t i = 0; i < sizeof(units) / sizeof(units[0]); i++) {
        int jitter = (int)(i % 5) - 2;
        add(ev, &n, (int)(units[i] * q + jitter * 4), 64 + (int)(i % 5), 95);
    }

    assert(mc_estimate_tempo(ev, n, 70, 180, &r));
    printf("tempo2 %u.%02u score %u onsets %u\n",
           mc_bpm_x100(r.bpm_q4) / 100, mc_bpm_x100(r.bpm_q4) % 100,
           r.score, r.onset_count);
    assert(r.bpm_q4 == 415);

    mc_event_t src = {0x00ABCDEFu, 0x92, 64, 111, 3}, dst = {0};
    uint8_t packed[MC_PACKED_EVENT_SIZE];
    assert(mc_pack_event7(packed, &src));
    mc_unpack_event7(&dst, packed);
    assert(dst.time_ticks == src.time_ticks);
    assert(dst.status == src.status && dst.data1 == src.data1);
    assert(dst.data2 == src.data2 && dst.flags == src.flags);

    mc_event_t too_late = {0x01000000u, 0x90, 60, 100, 0};
    assert(!mc_pack_event7(packed, &too_late));

    puts("midi_capture fixed-point: ok");
    return 0;
}
