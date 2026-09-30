#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../src/alpha_juno_randomizer.h"

typedef struct {
    unsigned param_count;
    unsigned voice_count;
    bool mono;
    uint8_t last_channel;
} capture_t;

static void capture_param(void *ctx, uint8_t ch, uint8_t param, uint8_t value)
{
    (void)param;
    (void)value;
    capture_t *c = (capture_t *)ctx;
    c->param_count++;
    c->last_channel = ch;
}

static void capture_voice(void *ctx, uint8_t ch, bool mono)
{
    capture_t *c = (capture_t *)ctx;
    c->voice_count++;
    c->mono = mono;
    c->last_channel = ch;
}

int main(void)
{
    uint8_t msg[10] = {0};
    const uint8_t expected[10] = {
        0xF0, 0x41, 0x36, 0x02, 0x23, 0x20, 0x01, 0x10, 0x63, 0xF7
    };

    assert(aj_build_param_sysex(msg, 2, 0x10, 0x63) == 10);
    assert(memcmp(msg, expected, sizeof(expected)) == 0);

    capture_t c = {0};
    aj_randomizer_io_t io = {
        .ctx = &c,
        .send_param = capture_param,
        .send_voice_mode = capture_voice,
    };

    assert(aj_randomizer_generate(AJ_CMD_BASS, 2, 0x12345678u, &io));
    assert(c.param_count == 34);
    assert(c.voice_count == 1);
    assert(c.mono == true);
    assert(c.last_channel == 2);

    puts("alpha_juno_randomizer: ok");
    return 0;
}
