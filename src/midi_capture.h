#ifndef MAX49_MIDI_CAPTURE_H
#define MAX49_MIDI_CAPTURE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define MC_TICK_HZ 15000u
#define MC_PACKED_EVENT_SIZE 7u
#define MC_PACKED_TIME_MAX 0xFFFFFFu
#define MC_DEFAULT_ONSET_CAPACITY 192u

typedef struct {
    uint32_t time_ticks;
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
    uint16_t bpm_q4;
    uint32_t score;
    uint16_t onset_count;
} mc_tempo_result_t;

void mc_ring_init(mc_ring_t *ring, mc_event_t *storage, uint16_t capacity);
void mc_ring_push(mc_ring_t *ring, uint32_t time_ticks, uint8_t status, uint8_t data1, uint8_t data2, uint8_t flags);
size_t mc_ring_copy_recent(const mc_ring_t *ring, mc_event_t *out, size_t out_capacity, uint32_t now_ticks, uint32_t window_ticks);

bool mc_estimate_tempo_onsets(const uint32_t *onsets, size_t onset_count,
                              uint16_t min_bpm, uint16_t max_bpm,
                              mc_tempo_result_t *result);
bool mc_estimate_tempo(const mc_event_t *events, size_t count,
                       uint16_t min_bpm, uint16_t max_bpm,
                       uint32_t *onset_scratch, size_t onset_capacity,
                       mc_tempo_result_t *result);
uint8_t mc_choose_bar_count(uint32_t phrase_ticks, uint16_t bpm_q4, uint8_t max_bars);
uint16_t mc_bpm_x100(uint16_t bpm_q4);

bool mc_pack_event7(uint8_t out[MC_PACKED_EVENT_SIZE], const mc_event_t *event);
void mc_unpack_event7(mc_event_t *event, const uint8_t in[MC_PACKED_EVENT_SIZE]);
#ifdef __cplusplus
}
#endif
#endif
