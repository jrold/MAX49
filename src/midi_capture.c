#include "midi_capture.h"

#include <math.h>

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

void mc_ring_push(mc_ring_t *ring, uint32_t time_ms, uint8_t status, uint8_t data1, uint8_t data2, uint8_t flags)
{
    if (!ring || !ring->events || ring->capacity == 0) return;
    mc_event_t *e = &ring->events[ring->write_index];
    e->time_ms = time_ms;
    e->status = status;
    e->data1 = data1;
    e->data2 = data2;
    e->flags = flags;
    ring->write_index = (uint16_t)((ring->write_index + 1u) % ring->capacity);
    if (ring->count < ring->capacity) ring->count++;
}

size_t mc_ring_copy_recent(const mc_ring_t *ring, mc_event_t *out, size_t out_capacity, uint32_t now_ms, uint32_t window_ms)
{
    if (!ring || !out || !ring->events || ring->capacity == 0) return 0;
    size_t n = 0;
    uint16_t oldest = (uint16_t)((ring->write_index + ring->capacity - ring->count) % ring->capacity);

    for (uint16_t i = 0; i < ring->count; ++i) {
        uint16_t idx = (uint16_t)((oldest + i) % ring->capacity);
        const mc_event_t *e = &ring->events[idx];
        uint32_t age = now_ms - e->time_ms;
        if (age <= window_ms) {
            if (n < out_capacity) out[n] = *e;
            n++;
        }
    }
    return n > out_capacity ? out_capacity : n;
}

/*
 * Free-play tempo estimator.
 *
 * Candidate tempos are scored using differences between Note-On timestamps,
 * which makes the score independent of the unknown bar/beat phase. Each IOI
 * is compared with the nearest integer multiple of a 1/16-note grid.
 */
bool mc_estimate_tempo(const mc_event_t *events, size_t count, float min_bpm, float max_bpm, mc_tempo_result_t *result)
{
    if (!events || !result || min_bpm < 30.0f || max_bpm <= min_bpm) return false;

    uint32_t onsets[192];
    size_t n = 0;
    for (size_t i = 0; i < count && n < (sizeof onsets / sizeof onsets[0]); ++i) {
        if (is_note_on(&events[i])) onsets[n++] = events[i].time_ms;
    }
    if (n < 4) return false;

    float best_bpm = 0.0f;
    float best_score = 1e30f;

    for (float bpm = min_bpm; bpm <= max_bpm + 0.001f; bpm += 0.25f) {
        float sixteenth_ms = 15000.0f / bpm;
        float total = 0.0f;
        float weight_sum = 0.0f;

        for (size_t i = 1; i < n; ++i) {
            size_t first = i > 6 ? i - 6 : 0;
            for (size_t j = first; j < i; ++j) {
                float dt = (float)(onsets[i] - onsets[j]);
                if (dt < 45.0f || dt > 4000.0f) continue;

                float units = dt / sixteenth_ms;
                float nearest = floorf(units + 0.5f);
                if (nearest < 1.0f) nearest = 1.0f;

                float err = fabsf(units - nearest);
                float weight = 1.0f / (1.0f + 0.12f * nearest);
                total += weight * err * err;

                /* Small complexity penalty helps break half/double-tempo ties
                 * in favor of a simpler rhythmic explanation. */
                total += weight * 0.00018f * nearest;
                weight_sum += weight;
            }
        }

        if (weight_sum == 0.0f) continue;
        float score = total / weight_sum;
        if (score < best_score) {
            best_score = score;
            best_bpm = bpm;
        }
    }

    if (best_bpm <= 0.0f) return false;
    result->bpm = best_bpm;
    result->score = best_score;
    result->onset_count = (uint16_t)n;
    return true;
}

uint8_t mc_choose_bar_count(uint32_t phrase_ms, float bpm, uint8_t max_bars)
{
    if (bpm <= 0.0f || max_bars == 0) return 0;

    const uint8_t choices[] = {1, 2, 4, 8, 16};
    float bar_ms = 240000.0f / bpm; /* 4/4 */
    uint8_t best = 1;
    float best_err = 1e30f;

    for (size_t i = 0; i < sizeof choices; ++i) {
        uint8_t bars = choices[i];
        if (bars > max_bars) break;

        float target = bar_ms * bars;
        float err = fabsf((float)phrase_ms - target) / target;
        if (err < best_err) {
            best_err = err;
            best = bars;
        }
    }
    return best;
}
