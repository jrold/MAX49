#include "midi_capture.h"

static bool is_note_on(const mc_event_t *e)
{
    return ((e->status & 0xF0u) == 0x90u) && e->data2 != 0;
}

void mc_ring_init(mc_ring_t *ring, mc_event_t *storage, uint16_t capacity)
{
    ring->events = storage;
    ring->capacity = capacity;
    ring->count = 0;
    ring->write_index = 0;
}

void mc_ring_push(mc_ring_t *ring, uint32_t time_ticks, uint8_t status, uint8_t data1, uint8_t data2, uint8_t flags)
{
    if (!ring || !ring->events || ring->capacity == 0) return;
    mc_event_t *e = &ring->events[ring->write_index];
    e->time_ticks = time_ticks;
    e->status = status;
    e->data1 = data1;
    e->data2 = data2;
    e->flags = flags;
    ring->write_index = (uint16_t)((ring->write_index + 1u) % ring->capacity);
    if (ring->count < ring->capacity) ring->count++;
}

size_t mc_ring_copy_recent(const mc_ring_t *ring, mc_event_t *out, size_t out_capacity, uint32_t now_ticks, uint32_t window_ticks)
{
    if (!ring || !out || !ring->events || ring->capacity == 0) return 0;
    size_t n = 0;
    uint16_t oldest = (uint16_t)((ring->write_index + ring->capacity - ring->count) % ring->capacity);

    for (uint16_t i = 0; i < ring->count; ++i) {
        uint16_t idx = (uint16_t)((oldest + i) % ring->capacity);
        const mc_event_t *e = &ring->events[idx];
        uint32_t age = now_ticks - e->time_ticks;
        if (age <= window_ticks) {
            if (n < out_capacity) out[n] = *e;
            n++;
        }
    }
    return n > out_capacity ? out_capacity : n;
}

bool mc_estimate_tempo(const mc_event_t *events, size_t count, uint16_t min_bpm, uint16_t max_bpm, mc_tempo_result_t *result)
{
    if (!events || !result || min_bpm < 30u || max_bpm <= min_bpm) return false;

    uint32_t onsets[192];
    size_t n = 0;
    for (size_t i = 0; i < count && n < (sizeof onsets / sizeof onsets[0]); ++i) {
        if (is_note_on(&events[i])) onsets[n++] = events[i].time_ticks;
    }
    if (n < 4) return false;

    uint32_t best_score = 0xFFFFFFFFu;
    uint16_t best_bpm_q4 = 0;

    for (uint16_t bpm_q4 = (uint16_t)(min_bpm * 4u); bpm_q4 <= (uint16_t)(max_bpm * 4u); ++bpm_q4) {
        /* A sixteenth note is 15/BPM seconds. With BPM represented as BPM*4:
         * step_ticks = tick_hz * 60 / bpm_q4. */
        uint32_t step_ticks = (MC_TICK_HZ * 60u + bpm_q4 / 2u) / bpm_q4;
        uint32_t total = 0;
        uint16_t pairs = 0;

        for (size_t i = 1; i < n; ++i) {
            size_t first = i > 6 ? i - 6 : 0;
            for (size_t j = first; j < i; ++j) {
                uint32_t dt = onsets[i] - onsets[j];
                if (dt < (MC_TICK_HZ * 45u) / 1000u || dt > MC_TICK_HZ * 4u) continue;

                uint32_t nearest = (dt + step_ticks / 2u) / step_ticks;
                if (nearest < 1u) nearest = 1u;

                uint32_t target = nearest * step_ticks;
                uint32_t err = dt > target ? dt - target : target - dt;
                uint32_t normalized = (err * 1024u + step_ticks / 2u) / step_ticks;

                /* Squared normalized grid error plus a small complexity penalty.
                 * The penalty breaks many half/double-tempo ties in favor of a
                 * rhythm explained by fewer sixteenth-note units. */
                total += normalized * normalized + nearest * 12u;
                pairs++;
            }
        }

        if (pairs == 0) continue;
        uint32_t score = total / pairs;
        if (score < best_score) {
            best_score = score;
            best_bpm_q4 = bpm_q4;
        }
    }

    if (best_bpm_q4 == 0) return false;
    result->bpm_q4 = best_bpm_q4;
    result->score = best_score;
    result->onset_count = (uint16_t)n;
    return true;
}

uint8_t mc_choose_bar_count(uint32_t phrase_ticks, uint16_t bpm_q4, uint8_t max_bars)
{
    static const uint8_t choices[] = {1, 2, 4, 8, 16};
    if (bpm_q4 == 0 || max_bars == 0) return 0;

    /* 4/4 bar = four quarter notes = 240/BPM seconds. */
    uint32_t bar_ticks = (MC_TICK_HZ * 960u + bpm_q4 / 2u) / bpm_q4;
    uint8_t best = 1;
    uint32_t best_error = 0xFFFFFFFFu;

    for (size_t i = 0; i < sizeof choices; ++i) {
        uint8_t bars = choices[i];
        if (bars > max_bars) break;
        uint32_t target = bar_ticks * bars;
        uint32_t error = phrase_ticks > target ? phrase_ticks - target : target - phrase_ticks;
        uint32_t relative = target ? (error * 4096u) / target : 0xFFFFFFFFu;
        if (relative < best_error) {
            best_error = relative;
            best = bars;
        }
    }
    return best;
}

uint16_t mc_bpm_x100(uint16_t bpm_q4)
{
    return (uint16_t)(bpm_q4 * 25u);
}

bool mc_pack_event7(uint8_t out[MC_PACKED_EVENT_SIZE], const mc_event_t *event)
{
    if (!out || !event || event->time_ticks > MC_PACKED_TIME_MAX) return false;
    out[0] = (uint8_t)(event->time_ticks);
    out[1] = (uint8_t)(event->time_ticks >> 8);
    out[2] = (uint8_t)(event->time_ticks >> 16);
    out[3] = event->status;
    out[4] = event->data1;
    out[5] = event->data2;
    out[6] = event->flags;
    return true;
}

void mc_unpack_event7(mc_event_t *event, const uint8_t in[MC_PACKED_EVENT_SIZE])
{
    if (!event || !in) return;
    event->time_ticks = (uint32_t)in[0] | ((uint32_t)in[1] << 8) | ((uint32_t)in[2] << 16);
    event->status = in[3];
    event->data1 = in[4];
    event->data2 = in[5];
    event->flags = in[6];
}
