#ifndef ALPHA_JUNO_RANDOMIZER_H
#define ALPHA_JUNO_RANDOMIZER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    AJ_CMD_INIT       = 0,
    AJ_CMD_RANDOM_ALL = 1,
    AJ_CMD_DARK       = 2,
    AJ_CMD_BRIGHT     = 3,
    AJ_CMD_BASS       = 8,
    AJ_CMD_LEAD       = 9,
    AJ_CMD_PAD        = 10,
    AJ_CMD_PLUCK      = 11,
    AJ_CMD_KICK       = 12,
    AJ_CMD_SNARE      = 13,
    AJ_CMD_HAT        = 14,
    AJ_CMD_FX         = 15
};

typedef void (*aj_send_param_fn)(
    void *ctx, uint8_t midi_channel, uint8_t param, uint8_t value);

typedef void (*aj_send_voice_mode_fn)(
    void *ctx, uint8_t midi_channel, bool mono);

typedef struct {
    void *ctx;
    aj_send_param_fn send_param;
    aj_send_voice_mode_fn send_voice_mode;
} aj_randomizer_io_t;

size_t aj_build_param_sysex(
    uint8_t out[10], uint8_t midi_channel, uint8_t param, uint8_t value);

bool aj_randomizer_generate(
    uint8_t command,
    uint8_t midi_channel,
    uint32_t seed,
    const aj_randomizer_io_t *io);

#ifdef __cplusplus
}
#endif

#endif
