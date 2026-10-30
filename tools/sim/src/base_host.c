// The base station's keypad logic (firmware/base/main/controls.c) behind a
// flat interface for the simulator: server.py builds this with the
// computer's C compiler into build/libbase.so and calls it through ctypes,
// so the simulated keypad runs the firmware's own code.
#include <string.h>
#include "controls.h"

static ctl_t ctl;

void base_init(unsigned autolock_ms) { ctl_init(&ctl, autolock_ms); }

// fins: the fin preset (0 following the body); fin_sensor: the fins follow their angle sensor
void base_set_live(int body, int eyes, int fins, int brightness, int fin_sensor)
{
    const ctl_scene_t live = { (int16_t)body, (int16_t)eyes, (int8_t)fins };
    ctl_set_live(&ctl, &live, (uint8_t)brightness, fin_sensor);
}

// WLED's layer slots: the body's and the fins' palettes, the layer presets over them (-1 not known)
void base_set_layers(int body_pal, int fins_pal, int body_layer, int fins_layer)
{
    const ctl_layers_t l = { (int16_t)body_pal, (int16_t)fins_pal, (int16_t)body_layer, (int16_t)fins_layer };
    ctl_set_layers(&ctl, &l);
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
// take_preview, latch, diff, brightness; layers: body palette, fins palette, body layer, fins layer
void base_state(int out[17])
{
    const int v[17] = { ctl.live.body, ctl.live.eyes, ctl.live.fins, ctl.standby.body, ctl.standby.eyes,
                        ctl.standby.fins, ctl.target, ctl.page, ctl.locked, ctl.take_preview, ctl.down[KEY_LATCH],
                        ctl_standby_diff(&ctl), ctl.brightness, ctl.layers.body_pal, ctl.layers.fins_pal,
                        ctl.layers.body_layer, ctl.layers.fins_layer };
    memcpy(out, v, sizeof(v));
}

void base_leds(unsigned now_ms, unsigned out[KEY_COUNT]) { ctl_led_colors(&ctl, now_ms, (uint32_t *)out); }
int base_key_count(void) { return KEY_COUNT; }
const char *base_key_name(int key) { return ctl_key_name((ctl_key_t)key); }
const char *base_target_name(int t) { return ctl_target_name((ctl_target_t)t); }
const char *base_page_name(int p) { return ctl_page_name(p); }
// The palette page's palette on key n (1-8) and its name ("" if not one of the defaults)
int base_palette(int n) { return n >= 1 && n <= CTL_PAGE_SIZE ? ctl.palettes[n - 1] : -1; }
const char *base_palette_name(int pal) { const char *s = ctl_palette_name(&ctl, pal); return s ? s : ""; }
