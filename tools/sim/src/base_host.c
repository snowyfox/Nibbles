// The base station's keypad logic (firmware/base/main/controls.c) behind a
// flat interface for the simulator: server.py builds this with the
// computer's C compiler into build/libbase.so and calls it through ctypes,
// so the simulated keypad runs the firmware's own code.
#include <string.h>
#include "controls.h"

static ctl_t ctl;

void base_init(unsigned autolock_ms) { ctl_init(&ctl, autolock_ms); }

void base_set_live(int body, int eyes, int fins, int brightness)
{
    const ctl_scene_t live = { (int16_t)body, (int16_t)eyes, (int8_t)fins };
    ctl_set_live(&ctl, &live, (uint8_t)brightness);
}

// Actions as 4 ints each: type, target, bump, value. Returns how many.
static int flat(const ctl_action_t *a, int n, int *out)
{
    for (int i = 0; i < n; i++) {
        out[i * 4] = a[i].type;
        out[i * 4 + 1] = a[i].target;
        out[i * 4 + 2] = a[i].bump;
        out[i * 4 + 3] = a[i].value;
    }
    return n;
}

int base_key(int key, int down, unsigned now_ms, int *out)
{
    ctl_action_t a[CTL_MAX_ACTIONS];
    return flat(a, ctl_key(&ctl, (ctl_key_t)key, down != 0, now_ms, a), out);
}

int base_tick(unsigned now_ms, int *out)
{
    ctl_action_t a[CTL_MAX_ACTIONS];
    return flat(a, ctl_tick(&ctl, now_ms, a), out);
}

// live body, eyes, fins; standby body, eyes, fins; target, page, locked,
// take_preview, latch, diff, brightness
void base_state(int out[13])
{
    const int v[13] = { ctl.live.body, ctl.live.eyes, ctl.live.fins, ctl.standby.body, ctl.standby.eyes,
                        ctl.standby.fins, ctl.target, ctl.page, ctl.locked, ctl.take_preview, ctl.down[KEY_LATCH],
                        ctl_standby_diff(&ctl), ctl.brightness };
    memcpy(out, v, sizeof(v));
}

void base_leds(unsigned now_ms, unsigned out[KEY_COUNT]) { ctl_led_colors(&ctl, now_ms, (uint32_t *)out); }
int base_key_count(void) { return KEY_COUNT; }
const char *base_key_name(int key) { return ctl_key_name((ctl_key_t)key); }
const char *base_target_name(int t) { return ctl_target_name((ctl_target_t)t); }
