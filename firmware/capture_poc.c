#include <stdint.h>
#include <stddef.h>

typedef int (*key_fn_t)(uint32_t, uint32_t, uint32_t);
typedef int (*note_fn_t)(uint32_t, uint32_t, uint32_t, uint32_t);
typedef uint16_t (*tick_fn_t)(void);

#define ORIG_KEY_UP     ((key_fn_t)(uintptr_t)0x08011F25u)
#define ORIG_KEY_DOWN   ((key_fn_t)(uintptr_t)0x08011FADu)
#define SEND_NOTE_OFF   ((note_fn_t)(uintptr_t)0x080141ABu)
#define SEND_NOTE_ON    ((note_fn_t)(uintptr_t)0x080141F3u)
#define GET_TICK16      ((tick_fn_t)(uintptr_t)0x08013F95u)

#define STATE_ADDR      0x20005000u
#define EVENT_ADDR      0x20005020u
#define STOCK_KEY_STATE 0x20000FC0u
#define EVENT_CAPACITY  1360u

#define KEY_CLEAR       0u
#define KEY_REPLAY      48u
#define EVENT_ON        1u
#define EVENT_OFF       0u
#define STATE_MAGIC     0x4D583439u /* "MX49" */

typedef struct __attribute__((packed)) {
    uint16_t dt;
    uint8_t note;
    uint8_t velocity;
    uint8_t channel;
    uint8_t type;
} event_t;

typedef struct {
    uint32_t magic;
    uint16_t count;
    uint16_t last_tick;
    uint8_t active;
    uint8_t have_event;
    uint8_t playing;
    uint8_t overflow;
} state_t;

static volatile state_t *const S = (volatile state_t *)(uintptr_t)STATE_ADDR;
static volatile event_t *const E = (volatile event_t *)(uintptr_t)EVENT_ADDR;
static volatile uint8_t *const K = (volatile uint8_t *)(uintptr_t)STOCK_KEY_STATE;

static inline uint16_t now16(void) { return GET_TICK16(); }

static void ensure_state(void)
{
    if (S->magic != STATE_MAGIC) {
        S->magic = STATE_MAGIC;
        S->count = 0;
        S->last_tick = 0;
        S->active = 0;
        S->have_event = 0;
        S->playing = 0;
        S->overflow = 0;
    }
}

static void reset_capture(void)
{
    S->count = 0;
    S->last_tick = now16();
    S->active = 1;
    S->have_event = 0;
    S->playing = 0;
    S->overflow = 0;
}

static void append_event(uint16_t t, uint8_t type, uint8_t note,
                         uint8_t velocity, uint8_t channel)
{
    if (!S->active || S->playing || S->overflow) return;

    uint16_t i = S->count;
    if (i >= EVENT_CAPACITY) {
        S->overflow = 1;
        S->active = 0;
        return;
    }

    uint16_t dt = S->have_event ? (uint16_t)(t - S->last_tick) : 0;
    E[i].dt = dt;
    E[i].note = note;
    E[i].velocity = velocity;
    E[i].channel = (uint8_t)(channel & 0x0Fu);
    E[i].type = type;

    S->last_tick = t;
    S->have_event = 1;
    S->count = (uint16_t)(i + 1u);
}

static void wait_ticks(uint16_t ticks)
{
    uint16_t start = now16();
    while ((uint16_t)(now16() - start) < ticks) {
        __asm__ volatile ("nop");
    }
}

static void replay_capture(void)
{
    if (S->count == 0 || S->playing) return;

    S->active = 0;
    S->playing = 1;
    uint16_t n = S->count;

    for (uint16_t i = 0; i < n; ++i) {
        event_t ev;
        ev.dt = E[i].dt;
        ev.note = E[i].note;
        ev.velocity = E[i].velocity;
        ev.channel = E[i].channel;
        ev.type = E[i].type;

        wait_ticks(ev.dt);

        if (ev.type == EVENT_ON) {
            (void)SEND_NOTE_ON(ev.channel, ev.note, ev.velocity, 1u);
        } else {
            (void)SEND_NOTE_OFF(ev.channel, ev.note, 0u, 1u);
        }
    }

    S->playing = 0;
}

__attribute__((used, noinline))
int hook_key_down(uint32_t key, uint32_t velocity, uint32_t context)
{
    ensure_state();
    uint16_t t = now16();

    if (key == KEY_CLEAR) {
        reset_capture();
        return 1;
    }

    if (key == KEY_REPLAY) {
        replay_capture();
        return 1;
    }

    int ret = ORIG_KEY_DOWN(key, velocity, context);

    if (S->active && !S->playing && context == 0u && key < 49u) {
        uint8_t channel = K[key * 2u + 0u];
        uint8_t note = K[key * 2u + 1u];
        if (note < 128u) {
            append_event(t, EVENT_ON, note, (uint8_t)velocity, channel);
        }
    }

    return ret;
}

__attribute__((used, noinline))
int hook_key_up(uint32_t key, uint32_t velocity, uint32_t context)
{
    ensure_state();
    (void)velocity;

    if (key == KEY_CLEAR || key == KEY_REPLAY) return 1;

    uint16_t t = now16();
    uint8_t channel = 0u;
    uint8_t note = 0xFFu;

    if (context == 0u && key < 49u) {
        channel = K[key * 2u + 0u];
        note = K[key * 2u + 1u];
    }

    int ret = ORIG_KEY_UP(key, 0u, context);

    if (S->active && !S->playing && note < 128u) {
        append_event(t, EVENT_OFF, note, 0u, channel);
    }

    return ret;
}
