#ifndef MAX49_MIDI_CAPTURE_H
#define MAX49_MIDI_CAPTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t time_ms;
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    uint8_t flags;
} mc_event_t;

typedef struct {
    mc_event_t *events;
    uint16_t capacity;
    uint16_t count;
    uint16_t write_index;
} mc_ring_t;

typedef struct {
    float bpm;
    float score;
    uint16_t onset_count;
} mc_tempo_result_t;

void mc_ring_init(mc_ring_t *ring, mc_event_t *storage, uint16_t capacity);
void mc_ring_push(mc_ring_t *ring, uint32_t time_ms, uint8_t status, uint8_t data1, uint8_t data2, uint8_t flags);
size_t mc_ring_copy_recent(const mc_ring_t *ring, mc_event_t *out, size_t out_capacity, uint32_t now_ms, uint32_t window_ms);

bool mc_estimate_tempo(const mc_event_t *events, size_t count, float min_bpm, float max_bpm, mc_tempo_result_t *result);
uint8_t mc_choose_bar_count(uint32_t phrase_ms, float bpm, uint8_t max_bars);

#ifdef __cplusplus
}
#endif
#endif
