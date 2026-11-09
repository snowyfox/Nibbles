// Host tests for the base station's keypad logic and power arithmetic.
//   make -C firmware/base/test/host
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "controls.h"
#include "nibbles_link.h"
#include "power_math.h"
#include "wled_palettes.h"
#include "log_format.h"
#include "power_sim.h"
#include "eye_thumb.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static ctl_action_t acts[CTL_MAX_ACTIONS];
static int nacts;
static uint32_t now;

static int press(ctl_t *c, ctl_key_t k) { now += 20; return nacts = ctl_key(c, k, true, now, acts); }
static int release(ctl_t *c, ctl_key_t k) { now += 20; return nacts = ctl_key(c, k, false, now, acts); }
static int tap(ctl_t *c, ctl_key_t k) { press(c, k); return release(c, k); }

static bool has(ctl_act_type_t type, int target, int value)
{
    for (int i = 0; i < nacts; i++)
        if (acts[i].type == type && (target < 0 || acts[i].target == target) && (value < -1000 || acts[i].value == value))
            return true;
    return false;
}

static void live(ctl_t *c, int body, int eyes, int fins)
{
    const ctl_scene_t s = { body, eyes, fins };
    ctl_set_live(c, &s, 128, 1);
}

static void test_standby_fills_from_live(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    CHECK(!c.standby_set);
    live(&c, 3, 0, -1);           // WLED heard first
    CHECK(c.standby.body == 3 && c.standby.eyes == 0 && !c.standby_set);
    live(&c, 3, 5, 0);            // then the eyes (the fins follow the body)
    CHECK(c.standby.body == 3 && c.standby.eyes == 5 && c.standby.fins == 0 && c.standby_set);
    CHECK(ctl_standby_diff(&c) == 0);
    live(&c, 4, 5, 0);            // Live moving on by itself doesn't drag Standby along
    CHECK(c.standby.body == 3);
}

static void test_number_is_a_bump(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    // ALL: holding 4 shows preset 4 on both while held, and nothing else changes
    press(&c, KEY_NUM4);
    CHECK(has(ACT_BUMP, TGT_BODY, 4) && has(ACT_BUMP, TGT_EYES, 4));
    CHECK(acts[0].bump == NL_BUMP_PRESET);
    CHECK(!has(ACT_PRESET, -1, -10000));
    release(&c, KEY_NUM4);
    CHECK(has(ACT_BUMP_END, TGT_BODY, -10000) && has(ACT_BUMP_END, TGT_EYES, -10000));
    CHECK(c.live.body == 1 && c.standby.body == 1);
}

static void test_latch_number(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_NUM5);
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_BODY, 5) && has(ACT_PRESET, TGT_EYES, 5));
    CHECK(acts[0].type == ACT_PRESET && acts[nacts - 1].type == ACT_BUMP_END);  // commands before the bumps let go
    CHECK(acts[0].target == TGT_BODY && acts[0].bump == 1);   // ALL: the WLED preset's eye settings may come too
    release(&c, KEY_LATCH);
    CHECK(release(&c, KEY_NUM5) == 0);          // nothing changes back
    CHECK(c.live.body == 5 && c.live.eyes == 5);
    CHECK(c.standby.body == 1 && c.standby.eyes == 1);   // the old Live, one TAKE away
}

static void test_take_is_a_bump_latch_swaps(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_NUM3);                        // 3 Live, 1 in Standby
    press(&c, KEY_LATCH);
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM3);
    // TAKE alone: Standby while held, back after
    press(&c, KEY_TAKE);
    CHECK(has(ACT_BUMP, TGT_BODY, 1) && has(ACT_BUMP, TGT_EYES, 1));
    release(&c, KEY_TAKE);
    CHECK(!has(ACT_PRESET, -1, -10000) && has(ACT_BUMP_END, TGT_BODY, -10000));
    CHECK(c.live.body == 3 && c.standby.body == 1);
    // TAKE + LATCH: the swap sticks
    press(&c, KEY_TAKE);
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_BODY, 1) && has(ACT_PRESET, TGT_EYES, 1));
    CHECK(acts[nacts - 1].type == ACT_BUMP_END);
    release(&c, KEY_LATCH);
    CHECK(release(&c, KEY_TAKE) == 0);
    CHECK(c.live.body == 1 && c.standby.body == 3);
}

static void test_take_with_nothing_staged(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    CHECK(press(&c, KEY_TAKE) == 0);
    CHECK(release(&c, KEY_TAKE) == 0);
    CHECK(press(&c, KEY_LATCH) == 0);           // LATCH with nothing held: nothing
    release(&c, KEY_LATCH);
}

static void test_latch_first_cues_standby(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_LATCH);
    CHECK(tap(&c, KEY_NUM7) == 0 && nacts == 0);
    release(&c, KEY_LATCH);
    CHECK(c.standby.body == 7 && c.standby.eyes == 7 && c.live.body == 1);
    // LATCH + TAKE: Standby back to Live
    press(&c, KEY_LATCH);
    tap(&c, KEY_TAKE);
    release(&c, KEY_LATCH);
    CHECK(c.standby.body == 1 && ctl_standby_diff(&c) == 0);
}

static void test_targets(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    tap(&c, KEY_EYES);                          // tap latches the target
    CHECK(c.target == TGT_EYES);
    press(&c, KEY_NUM6);
    CHECK(nacts == 1 && acts[0].target == TGT_EYES && acts[0].value == 6);
    tap(&c, KEY_LATCH);
    release(&c, KEY_NUM6);
    CHECK(c.live.eyes == 6 && c.live.body == 1);
    // hold BODY + a number: momentary target, back to EYES after
    press(&c, KEY_BODY);
    press(&c, KEY_NUM3);
    CHECK(nacts == 1 && acts[0].target == TGT_BODY);
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_BODY, 3) && acts[0].bump == 0);   // BODY: body only
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM3);
    release(&c, KEY_BODY);
    CHECK(c.target == TGT_EYES && c.live.body == 3);
    // FINS: a number bumps that fin preset; LATCH keeps it
    tap(&c, KEY_FINS);
    press(&c, KEY_NUM2);
    CHECK(nacts == 1 && acts[0].type == ACT_BUMP && acts[0].target == TGT_FINS && acts[0].value == 2);
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_FINS, 2) && c.live.fins == 2 && c.standby.fins == 0);
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM2);
    CHECK(c.live.body == 3);                    // the body kept its preset
}

static void test_pages_and_brightness(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_NUM9);                        // numbers 9-16 are presets 9-16
    CHECK(has(ACT_BUMP, TGT_BODY, 9));
    release(&c, KEY_NUM9);
    // PAGE held + number: brightness, and the page doesn't flip
    press(&c, KEY_PAGE);
    press(&c, KEY_NUM8);
    CHECK(has(ACT_BRIGHTNESS, -1, 255));
    release(&c, KEY_NUM8);
    press(&c, KEY_NUM4);
    CHECK(has(ACT_BRIGHTNESS, -1, 127));
    release(&c, KEY_NUM4);
    CHECK(press(&c, KEY_NUM12) == 0);           // no twelfth eighth
    release(&c, KEY_NUM12);
    release(&c, KEY_PAGE);
    CHECK(c.page == PAGE_PRESETS);
    tap(&c, KEY_PAGE);
    CHECK(c.page == PAGE_PALETTES);
    tap(&c, KEY_PAGE);
    CHECK(c.page == PAGE_LAYERS);
    tap(&c, KEY_PAGE);
    CHECK(c.page == PAGE_PRESETS);
}

// The palette viewer: all of WLED's palettes in a list that goes round,
// CTL_PALETTE_ROWS on the screen; UP/DOWN scroll by one (by themselves while
// held), PAGEUP/PAGEDOWN and LEFT/RIGHT by a screenful.
static void test_palette_scroll(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    const int R = CTL_PALETTE_ROWS, N = WLED_PALETTE_COUNT;
    tap(&c, KEY_DOWN);                          // no palettes showing: the arrows do nothing
    CHECK(c.pal_first == 0);
    press(&c, KEY_MOD_PALETTE);
    tap(&c, KEY_DOWN);
    CHECK(c.pal_first == 1 && ctl_set_palette(&c, 1) == 1 && ctl_set_palette(&c, R) == R);
    press(&c, KEY_NUM2);
    CHECK(has(ACT_BUMP, TGT_BODY, nl_slot_arg(NL_SLOT_BODY, 2)));
    release(&c, KEY_NUM2);
    CHECK(ctl_set_palette(&c, R + 1) == -1);
    tap(&c, KEY_PAGE_DOWN);
    CHECK(c.pal_first == 1 + R);
    tap(&c, KEY_PAGE_UP);
    tap(&c, KEY_LEFT);                          // round to the end: the last palettes, then the first
    CHECK(c.pal_first == N + 1 - R);
    CHECK(ctl_set_palette(&c, R) == 0 && ctl_set_palette(&c, R - 1) == N - 1);
    tap(&c, KEY_RIGHT);
    tap(&c, KEY_UP);
    CHECK(c.pal_first == 0);
    tap(&c, KEY_UP);
    CHECK(c.pal_first == N - 1);
    // DOWN held: one at once, then by itself after the delay, faster later
    c.pal_first = 0;
    press(&c, KEY_DOWN);
    const uint32_t t0 = now;
    CHECK(c.pal_first == 1);
    for (now = t0; now < t0 + CTL_SCROLL_DELAY_MS - 50; now += 50) ctl_tick(&c, now, acts);
    CHECK(c.pal_first == 1);
    for (; now < t0 + CTL_SCROLL_DELAY_MS + 3 * CTL_SCROLL_REPEAT_MS; now += 10) ctl_tick(&c, now, acts);
    CHECK(c.pal_first == 4);                    // one at the delay, two more a repeat apart
    for (; now < t0 + CTL_SCROLL_FAST_MS; now += 10) ctl_tick(&c, now, acts);
    const int slow = c.pal_first;
    for (; now < t0 + CTL_SCROLL_FAST_MS + 10 * CTL_SCROLL_FAST_REPEAT_MS; now += 10) ctl_tick(&c, now, acts);
    CHECK(c.pal_first - slow >= 9);             // faster now
    release(&c, KEY_DOWN);
    const int stopped = c.pal_first;
    now += 1000;
    ctl_tick(&c, now, acts);
    CHECK(c.pal_first == stopped);
    release(&c, KEY_MOD_PALETTE);
    // the page works the same way
    c.page = PAGE_PALETTES;
    tap(&c, KEY_DOWN);
    CHECK(c.pal_first == stopped + 1);
    CHECK(!strcmp(ctl_palette_name(&c, 44), "Orange & Teal") && ctl_palette_name(&c, WLED_PALETTE_COUNT) == NULL);
}

// Modifiers held change what the numbers do, as hotkeys; keys of their own
// do one thing whatever is held.
static void test_modifiers_and_own_keys(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    const ctl_layers_t l0 = { 11, -1, 0, 0 };
    ctl_set_layers(&c, &l0);
    // LAYER held + 12: layer look 12, bump then LATCH, as on the layer page
    press(&c, KEY_MOD_LAYER);
    CHECK(ctl_mode(&c) == PAGE_LAYERS && c.page == PAGE_PRESETS);
    press(&c, KEY_NUM12);
    CHECK(has(ACT_BUMP, TGT_BODY, nl_slot_arg(NL_SLOT_BODY_OVER, NL_LAYER_PRESET_BASE + 12)));
    release(&c, KEY_MOD_LAYER);                 // letting go of the modifier first changes nothing
    press(&c, KEY_LATCH);
    CHECK(has(ACT_SLOT_LOOK, NL_SLOT_BODY_OVER, NL_LAYER_PRESET_BASE + 12));
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM12);
    CHECK(c.layers.body_layer == NL_LAYER_PRESET_BASE + 12 && ctl_mode(&c) == PAGE_PRESETS);
    // PALETTE held + 3: the viewer's third palette (WLED palette 2 in the first set)
    press(&c, KEY_MOD_PALETTE);
    press(&c, KEY_NUM3);
    CHECK(has(ACT_BUMP, TGT_BODY, nl_slot_arg(NL_SLOT_BODY, 2)));
    release(&c, KEY_NUM3);
    uint32_t rgb[KEY_COUNT];
    ctl_led_colors(&c, now, rgb);
    CHECK(rgb[KEY_NUM3] != 0 && rgb[KEY_MOD_PALETTE] == 0xffffff && rgb[KEY_DOWN] != 0);
    CHECK((rgb[KEY_NUM9] != 0) == (CTL_PALETTE_ROWS >= 9));   // numbers past the set are dark
    release(&c, KEY_MOD_PALETTE);
    ctl_led_colors(&c, now, rgb);
    CHECK(rgb[KEY_DOWN] == 0);                  // the arrows are dark with no palettes showing
    // BRIGHT held: brightness
    press(&c, KEY_MOD_BRIGHT);
    press(&c, KEY_NUM2);
    CHECK(has(ACT_BRIGHTNESS, -1, 63));
    release(&c, KEY_NUM2);
    release(&c, KEY_MOD_BRIGHT);
    // Keys of their own ignore the page and the modifiers
    c.page = PAGE_LAYERS;
    press(&c, KEY_MOD_PALETTE);
    press(&c, KEY_PRESET1 + 13);                // PRESET14
    CHECK(has(ACT_BUMP, TGT_BODY, 14) && has(ACT_BUMP, TGT_EYES, 14));
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_BODY, 14) && c.live.body == 14);
    release(&c, KEY_LATCH);
    release(&c, KEY_PRESET1 + 13);
    release(&c, KEY_MOD_PALETTE);
    press(&c, KEY_PALETTE1 + 4);
    CHECK(has(ACT_BUMP, TGT_BODY, nl_slot_arg(NL_SLOT_BODY, c.palettes[4])));
    release(&c, KEY_PALETTE1 + 4);
    press(&c, KEY_LATCH);                       // LATCH first: cue a layer look in Standby
    CHECK(press(&c, KEY_LAYER1 + 15) == 0 && c.standby_layers.body_layer == NL_LAYER_PRESET_BASE + 16);
    release(&c, KEY_LAYER1 + 15);
    release(&c, KEY_LATCH);
    press(&c, KEY_BRIGHT1 + 7);
    CHECK(has(ACT_BRIGHTNESS, -1, 255));
    release(&c, KEY_BRIGHT1 + 7);
    CHECK(press(&c, KEY_POLE_POWER) == 1 && acts[0].type == ACT_POLE_POWER);
    release(&c, KEY_POLE_POWER);
    // LEDs: the live preset's own key is red on any page
    ctl_led_colors(&c, now, rgb);
    CHECK(rgb[KEY_PRESET1 + 13] == 0xff1010 && rgb[KEY_PRESET1] != 0xff1010);
}

static void layers(ctl_t *c, int body_pal, int fins_pal, int body_layer, int fins_layer)
{
    const ctl_layers_t l = { body_pal, fins_pal, body_layer, fins_layer };
    ctl_set_layers(c, &l);
}

static void test_palettes(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);            // the fins follow the body
    layers(&c, 11, -1, 0, 0);
    c.page = PAGE_PALETTES;
    // ALL: holding 2 shows the second palette on the body only (the fins follow it)
    press(&c, KEY_NUM2);
    CHECK(nacts == 1 && acts[0].type == ACT_BUMP && acts[0].target == TGT_BODY && acts[0].bump == NL_BUMP_PALETTE);
    CHECK(nl_slot_arg_slot(acts[0].value) == NL_SLOT_BODY && nl_slot_arg_value(acts[0].value) == 1);
    release(&c, KEY_NUM2);
    CHECK(has(ACT_BUMP_END, TGT_BODY, -10000));
    CHECK(c.layers.body_pal == 11);           // a bump changes nothing
    // LATCH keeps it: the command, then the bump lets go
    press(&c, KEY_NUM3);
    press(&c, KEY_LATCH);
    CHECK(acts[0].type == ACT_SLOT_PALETTE && acts[0].target == NL_SLOT_BODY && acts[0].value == 2);
    CHECK(acts[nacts - 1].type == ACT_BUMP_END);
    release(&c, KEY_LATCH);
    CHECK(release(&c, KEY_NUM3) == 0);
    CHECK(c.layers.body_pal == 2 && c.live.body == 1);   // the preset stays
    // Fins with their own preset: ALL covers them too; FINS alone only them
    live(&c, 1, 1, 3);
    press(&c, KEY_NUM1);
    CHECK(nacts == 2 && has(ACT_BUMP, TGT_FINS, nl_slot_arg(NL_SLOT_FINS, 0)));
    release(&c, KEY_NUM1);
    press(&c, KEY_FINS);
    press(&c, KEY_NUM1);
    CHECK(nacts == 1 && has(ACT_BUMP, TGT_FINS, -10000));
    release(&c, KEY_NUM1);
    release(&c, KEY_FINS);
    // EYES: nothing to recolour
    press(&c, KEY_EYES);
    CHECK(press(&c, KEY_NUM1) == 0);
    release(&c, KEY_NUM1);
    release(&c, KEY_EYES);
    // LATCH first cues the palette in Standby, unseen
    press(&c, KEY_LATCH);
    CHECK(press(&c, KEY_NUM4) == 0 && c.standby.body == 1 && c.standby_layers.body_pal == 3);
    release(&c, KEY_NUM4);
    release(&c, KEY_LATCH);
}

static ctl_action_t *find(ctl_act_type_t type, int target)
{
    for (int i = 0; i < nacts; i++)
        if (acts[i].type == type && (target < 0 || acts[i].target == target)) return &acts[i];
    return NULL;
}

// Palettes and layer looks in Standby: cue, preview with TAKE, take with LATCH.
static void test_standby_layers(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    layers(&c, 11, -1, 0, 0);
    c.page = PAGE_PALETTES;
    // LATCH held, then 2: Lava cued for the body; nothing sent; TAKE lights up
    press(&c, KEY_LATCH);
    CHECK(press(&c, KEY_PALETTE1 + 1) == 0 && c.standby_layers.body_pal == 8);
    release(&c, KEY_PALETTE1 + 1);
    release(&c, KEY_LATCH);
    CHECK(ctl_standby_diff(&c) == (1 << TGT_BODY));
    uint32_t rgb[KEY_COUNT];
    ctl_led_colors(&c, now, rgb);
    CHECK(rgb[KEY_PALETTE1 + 1] == 0x00e040 && rgb[KEY_PALETTE1 + 3] == 0xff1010);   // Standby green, Live (Rainbow 11) red
    // TAKE previews it: one scene bump on the body, palette + 1, preset and layer left alone
    press(&c, KEY_TAKE);
    CHECK(nacts == 1 && acts[0].type == ACT_BUMP && acts[0].target == TGT_BODY && acts[0].bump == NL_BUMP_SCENE);
    CHECK(acts[0].value == nl_scene_arg(0, 0) && acts[0].extra == 9);
    layers(&c, 8, -1, 0, 0);                  // WLED reports the preview as live
    press(&c, KEY_LATCH);                     // keep it: the palette, then the bump lets go
    CHECK(nacts == 2 && acts[0].type == ACT_SLOT_PALETTE && acts[0].target == NL_SLOT_BODY && acts[0].value == 8);
    CHECK(acts[1].type == ACT_BUMP_END);
    release(&c, KEY_LATCH);
    CHECK(release(&c, KEY_TAKE) == 0);
    CHECK(c.layers.body_pal == 8 && c.standby_layers.body_pal == 11);   // the old palette, one TAKE away
    // TAKE without LATCH: a preview of Rainbow, then back
    press(&c, KEY_TAKE);
    CHECK(nacts == 1 && acts[0].bump == NL_BUMP_SCENE && acts[0].extra == 12);
    release(&c, KEY_TAKE);
    CHECK(nacts == 1 && acts[0].type == ACT_BUMP_END && c.layers.body_pal == 8);

    // A new preset in Standby comes with its own palette (the cue is cleared);
    // a layer look cued after it goes with it.
    c.page = PAGE_PRESETS;
    press(&c, KEY_LATCH);
    press(&c, KEY_NUM5);
    release(&c, KEY_NUM5);
    c.page = PAGE_LAYERS;
    press(&c, KEY_NUM3);
    release(&c, KEY_NUM3);
    release(&c, KEY_LATCH);
    CHECK(c.standby.body == 5 && c.standby_layers.body_pal == -1 && c.standby_layers.body_layer == NL_LAYER_PRESET_BASE + 3);
    press(&c, KEY_TAKE);
    const ctl_action_t *b = find(ACT_BUMP, TGT_BODY);
    CHECK(b && b->bump == NL_BUMP_SCENE && b->value == nl_scene_arg(5, 3) && b->extra == 0);
    CHECK(has(ACT_BUMP, TGT_EYES, 5) && !find(ACT_BUMP, TGT_FINS));
    press(&c, KEY_LATCH);
    // the preset, then its layer look, then the eyes, then the bumps let go
    CHECK(acts[0].type == ACT_PRESET && acts[0].target == TGT_BODY && acts[0].value == 5);
    CHECK(acts[1].type == ACT_SLOT_LOOK && acts[1].target == NL_SLOT_BODY_OVER && acts[1].value == NL_LAYER_PRESET_BASE + 3);
    CHECK(has(ACT_PRESET, TGT_EYES, 5) && acts[nacts - 1].type == ACT_BUMP_END);
    release(&c, KEY_LATCH);
    release(&c, KEY_TAKE);
    CHECK(c.live.body == 5 && c.layers.body_layer == NL_LAYER_PRESET_BASE + 3 && c.layers.body_pal == -1);
    CHECK(c.standby.body == 1 && c.standby_layers.body_pal == 8 && c.standby_layers.body_layer == 0);   // Live as it was
    // Taking it back puts preset 1 back with Lava and no layer
    layers(&c, 30, -1, NL_LAYER_PRESET_BASE + 3, 0);
    press(&c, KEY_TAKE);
    b = find(ACT_BUMP, TGT_BODY);
    CHECK(b && b->bump == NL_BUMP_SCENE && b->value == nl_scene_arg(1, NL_SCENE_LAYER_OFF) && b->extra == 9);
    release(&c, KEY_TAKE);

    // A layer look Standby already has is cued off; LATCH + TAKE clears the cues
    press(&c, KEY_LATCH);
    tap(&c, KEY_NUM3);
    CHECK(c.standby_layers.body_layer == NL_LAYER_PRESET_BASE + 3);
    tap(&c, KEY_NUM3);
    CHECK(c.standby_layers.body_layer == 0);
    tap(&c, KEY_TAKE);
    release(&c, KEY_LATCH);
    CHECK(c.standby.body == 5 && c.standby_layers.body_pal == -1 && c.standby_layers.body_layer == -1 && ctl_standby_diff(&c) == 0);

    // The fins on their own preset: a palette cued for FINS previews as a scene bump on the fins
    live(&c, 5, 5, 3);
    layers(&c, 30, 6, 0, 0);
    c.page = PAGE_PALETTES;
    press(&c, KEY_LATCH);                     // Standby = Live
    tap(&c, KEY_TAKE);
    release(&c, KEY_LATCH);
    press(&c, KEY_FINS);
    press(&c, KEY_LATCH);
    tap(&c, KEY_PALETTE1);                        // Party is already the fins' palette: nothing to take
    CHECK(ctl_standby_diff(&c) == 0);
    tap(&c, KEY_PALETTE1 + 1);
    release(&c, KEY_LATCH);
    release(&c, KEY_FINS);
    CHECK(ctl_standby_diff(&c) == (1 << TGT_FINS));
    press(&c, KEY_TAKE);
    CHECK(nacts == 1 && acts[0].target == TGT_FINS && acts[0].bump == NL_BUMP_SCENE && acts[0].value == nl_scene_arg(0, 0) && acts[0].extra == 9);
    press(&c, KEY_LATCH);
    CHECK(acts[0].type == ACT_SLOT_PALETTE && acts[0].target == NL_SLOT_FINS && acts[0].value == 8);
    release(&c, KEY_LATCH);
    release(&c, KEY_TAKE);

    // Latching a palette live drops the old one into Standby
    layers(&c, 30, 8, 0, 0);
    press(&c, KEY_PALETTE1 + 2);
    press(&c, KEY_LATCH);
    release(&c, KEY_LATCH);
    release(&c, KEY_PALETTE1 + 2);
    CHECK(c.layers.body_pal == c.palettes[2] && c.standby.body == 5 && c.standby_layers.body_pal == 30);
}

static void test_layers(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    layers(&c, 11, -1, 0, 0);
    c.page = PAGE_LAYERS;
    press(&c, KEY_NUM5);
    CHECK(nacts == 1 && acts[0].bump == NL_BUMP_LAYER && acts[0].target == TGT_BODY);
    CHECK(acts[0].value == nl_slot_arg(NL_SLOT_BODY_OVER, NL_LAYER_PRESET_BASE + 5));
    press(&c, KEY_LATCH);
    CHECK(acts[0].type == ACT_SLOT_LOOK && acts[0].target == NL_SLOT_BODY_OVER && acts[0].value == NL_LAYER_PRESET_BASE + 5);
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM5);
    CHECK(c.layers.body_layer == NL_LAYER_PRESET_BASE + 5);
    // the live one's LED is red; LATCH on it again switches the layer off
    uint32_t rgb[KEY_COUNT];
    ctl_led_colors(&c, now, rgb);
    CHECK(rgb[KEY_NUM5] == 0xff1010 && rgb[KEY_NUM4] != 0xff1010);
    press(&c, KEY_NUM5);
    press(&c, KEY_LATCH);
    CHECK(has(ACT_SLOT_LOOK, NL_SLOT_BODY_OVER, 0));
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM5);
    CHECK(c.layers.body_layer == 0);
    // While a layer bump shows, WLED reports that look as live: LATCH still keeps it
    press(&c, KEY_NUM6);
    layers(&c, 11, -1, NL_LAYER_PRESET_BASE + 6, 0);
    press(&c, KEY_LATCH);
    CHECK(has(ACT_SLOT_LOOK, NL_SLOT_BODY_OVER, NL_LAYER_PRESET_BASE + 6));
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM6);
    layers(&c, 11, -1, 0, 0);
    // FINS: the fins' overlay
    press(&c, KEY_FINS);
    press(&c, KEY_NUM2);
    CHECK(acts[0].target == TGT_FINS && nl_slot_arg_slot(acts[0].value) == NL_SLOT_FINS_OVER);
    release(&c, KEY_NUM2);
    release(&c, KEY_FINS);
}

static void test_fins_and_all(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 4);                          // the fins on their own preset 4
    // ALL + a number: the whole shark, so the fins show the body's preset while held...
    press(&c, KEY_NUM6);
    CHECK(has(ACT_BUMP, TGT_BODY, 6) && has(ACT_BUMP, TGT_EYES, 6) && has(ACT_BUMP, TGT_FINS, 0));
    // ...and LATCH puts them back to following the body
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_FINS, 0) && c.live.fins == 0 && c.standby.fins == 4);
    release(&c, KEY_LATCH);
    release(&c, KEY_NUM6);
    // TAKE swaps the fins too
    press(&c, KEY_TAKE);
    CHECK(has(ACT_BUMP, TGT_FINS, 4));
    press(&c, KEY_LATCH);
    CHECK(has(ACT_PRESET, TGT_FINS, 4) && c.live.fins == 4);
    release(&c, KEY_LATCH);
    release(&c, KEY_TAKE);
    // FIN SENSOR: the angle sensor, apart from the presets
    tap(&c, KEY_FIN_SENSOR);
    press(&c, KEY_FIN_SENSOR);
    CHECK(has(ACT_FINS, TGT_FINS, 1) || has(ACT_FINS, TGT_FINS, 0));
    release(&c, KEY_FIN_SENSOR);
    CHECK(c.live.fins == 4);
}

static void test_pole_power_key(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_PAGE);
    CHECK(press(&c, KEY_BLACKOUT) == 1 && acts[0].type == ACT_POLE_POWER);   // no blackout bump
    CHECK(c.bump_slots == 0);
    release(&c, KEY_BLACKOUT);
    int page = c.page;
    release(&c, KEY_PAGE);
    CHECK(c.page == page);                    // and no page flip
    press(&c, KEY_BLACKOUT);                  // alone it's still the bump
    CHECK(has(ACT_BUMP, TGT_BODY, -10000) && !has(ACT_POLE_POWER, -1, -10000));
    release(&c, KEY_BLACKOUT);
}

static void test_flash_blackout(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_FLASH);
    CHECK(has(ACT_BUMP, TGT_BODY, 0) && has(ACT_BUMP, TGT_EYES, 0) && acts[0].bump == NL_BUMP_FLASH);
    release(&c, KEY_FLASH);
    CHECK(nacts == 2 && acts[0].type == ACT_BUMP_END);
    tap(&c, KEY_EYES);
    press(&c, KEY_BLACKOUT);
    CHECK(nacts == 1 && acts[0].target == TGT_EYES && acts[0].bump == NL_BUMP_BLACKOUT);
    release(&c, KEY_BLACKOUT);
}

static void test_lock(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_UNLOCK);
    now += CTL_LOCK_HOLD_MS;
    ctl_tick(&c, now, acts);
    CHECK(c.locked);
    now += 500;
    ctl_tick(&c, now, acts);                    // still held: no second toggle
    CHECK(c.locked);
    release(&c, KEY_UNLOCK);
    CHECK(tap(&c, KEY_NUM3) == 0 && c.standby.body == 1);
    CHECK(press(&c, KEY_FLASH) == 0);
    release(&c, KEY_FLASH);
    uint32_t rgb[KEY_COUNT];
    ctl_led_colors(&c, now, rgb);
    CHECK(rgb[KEY_NUM1] == 0 && rgb[KEY_UNLOCK] != 0);
    press(&c, KEY_UNLOCK);
    now += CTL_LOCK_HOLD_MS;
    ctl_tick(&c, now, acts);
    CHECK(!c.locked);
    release(&c, KEY_UNLOCK);
    // locking while a bump is held releases it
    press(&c, KEY_FLASH);
    press(&c, KEY_UNLOCK);
    now += CTL_LOCK_HOLD_MS;
    nacts = ctl_tick(&c, now, acts);
    CHECK(c.locked && has(ACT_BUMP_END, TGT_BODY, -10000));
}

static void test_autolock(void)
{
    ctl_t c;
    ctl_init(&c, 60000);
    live(&c, 1, 1, 0);
    tap(&c, KEY_NUM2);
    now += 59000;
    ctl_tick(&c, now, acts);
    CHECK(!c.locked);
    now += 2000;
    ctl_tick(&c, now, acts);
    CHECK(c.locked);
}

static void test_leds(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    live(&c, 1, 1, 0);
    press(&c, KEY_LATCH);
    tap(&c, KEY_NUM3);
    release(&c, KEY_LATCH);
    uint32_t rgb[KEY_COUNT];
    ctl_led_colors(&c, 0, rgb);
    CHECK(rgb[KEY_NUM1] == 0xff1010);           // live
    CHECK(rgb[KEY_NUM3] == 0x00e040);           // staged
    CHECK(rgb[KEY_ALL] == 0xffffff);            // selected target
    CHECK(rgb[KEY_TAKE] != 0x202020);           // something to take
}

static void test_names(void)
{
    for (int k = 1; k < KEY_COUNT; k++) CHECK((int)ctl_key_by_name(ctl_key_name((ctl_key_t)k)) == k);
    CHECK(ctl_key_by_name("nope") == KEY_NONE);
}

static void test_log_format(void)
{
    CHECK(log_file_number("L00042.CSV") == 42 && log_file_number("l00001.csv") == 1);
    CHECK(log_file_number("L00042.TXT") == -1 && log_file_number("NOTES.CSV") == -1 && log_file_number("L0042.CSV") == -1);
    log_sample_t s = { .t_s = 12, .unix_time = 1793000000, .sim = true, .pack_mv = 15100, .pack_pct = 55, .pack_ma = -2300, .input = "usb", .cell_c10 = 253,
                       .die_c10 = INT16_MIN, .wled_preset = 230, .wled_bri = 255, .wled_fps = 43, .fin_follow = -1, .dropped = 3, .chg_limit_ma = 1500 };
    char row[320];
    const size_t n = log_row(row, sizeof(row), &s);
    CHECK(n > 0 && row[n - 1] == '\n');
    CHECK(!strcmp(row, "12,1793000000,1,15100,55,-2300,usb,0,0,0,0,0,0,0,0,25.3,,0,0,0,0,0,0,0,230,255,43,-1,3,1500\n"));
    s.cell_c10 = -45;
    log_row(row, sizeof(row), &s);
    CHECK(strstr(row, ",-4.5,,") != NULL);
    int cols = 1, head = 1;
    for (const char *p = row; *p; p++) cols += *p == ',';
    for (const char *p = LOG_HEADER; *p; p++) head += *p == ',';
    CHECK(cols == head);                       // a value for every heading
    CHECK(log_row(row, 10, &s) == 0);          // too small: nothing, not half a row
    // reading the charge back, for the graph after a restart
    uint32_t t = 0;
    int pct = 0;
    s.sim = false;
    log_row(row, sizeof(row), &s);
    CHECK(log_parse_charge(row, &t, &pct, false) && t == 1793000000 && pct == 55);
    CHECK(!log_parse_charge(LOG_HEADER, &t, &pct, true));
    s.sim = true;
    log_row(row, sizeof(row), &s);
    CHECK(!log_parse_charge(row, &t, &pct, false));   // made-up readings don't come back
    s.sim = false;
    s.unix_time = 0;
    log_row(row, sizeof(row), &s);
    CHECK(!log_parse_charge(row, &t, &pct, false));   // nor rows with no real time
    s.unix_time = 1793000000;
    s.pack_pct = -1;
    log_row(row, sizeof(row), &s);
    CHECK(!log_parse_charge(row, &t, &pct, false));   // nor no reading
    // rows from a build's simulation only count in such a build
    s.pack_pct = 55;
    s.sim = 2;
    log_row(row, sizeof(row), &s);
    CHECK(!log_parse_charge(row, &t, &pct, false));
    CHECK(log_parse_charge(row, &t, &pct, true) && pct == 55);
    s.sim = 1;
    log_row(row, sizeof(row), &s);
    CHECK(!log_parse_charge(row, &t, &pct, true));   // test readings never do
    int8_t hist[4];
    memset(hist, -1, sizeof(hist));
    log_history_add(hist, 4, 20.0f, 1000, 1000, 50);   // now: the newest column
    log_history_add(hist, 4, 20.0f, 1000, 959, 40);    // 41 s ago: two slices back
    log_history_add(hist, 4, 20.0f, 1000, 900, 30);    // too old
    log_history_add(hist, 4, 20.0f, 1000, 1100, 20);   // from the future
    CHECK(hist[3] == 50 && hist[1] == 40 && hist[0] == -1 && hist[2] == -1);
    // the clock chip's registers
    uint8_t r[7];
    rtc_unix_to_regs(1793554700, r);           // 2026-11-01 17:38:20 UTC, a Sunday
    CHECK(r[0] == 0x20 && r[1] == 0x38 && r[2] == 0x17 && r[3] == 0x01 && r[4] == 0 && r[5] == 0x11 && r[6] == 0x26);
    CHECK(rtc_regs_to_unix(r) == 1793554700);
    rtc_unix_to_regs(1835481600, r);           // 2028-03-01: past a leap day
    CHECK(r[3] == 0x01 && r[5] == 0x03 && r[6] == 0x28 && rtc_regs_to_unix(r) == 1835481600);
    r[0] |= 0x80;
    CHECK(rtc_regs_to_unix(r) == 0);           // the oscillator stopped
    r[0] &= 0x7F; r[5] = 0x13;
    CHECK(rtc_regs_to_unix(r) == 0);           // not a date
}

static void test_power_sim(void)
{
    for (int p = 0; p <= 100; p += 5) CHECK(abs(pack_percent(sim_rest_mv(p), 0) - p) <= 1);

    // Fast: everything the screen can show, within a cycle.
    bool seen_in[3] = { 0 }, fault = false, efuse = false, hot = false, cold = false, not_set = false, starved = false, done = false;
    int lo_pct = 100, hi_pct = 0, hi_die = 0, hi_cell = 0, lo_cell = 1000, hi_ma = 0;
    for (double t = 0; t < SIM_FAST_CYCLE_S; t += 0.2) {
        sim_reading_t r;
        sim_fast(t, &r);
        seen_in[input_kind(r.vbus_mv)] = true;
        fault |= r.fault0 != 0;
        efuse |= !r.efuse_pg;
        hot |= r.ts_flags == 1;
        cold |= r.ts_flags == 8;
        not_set |= !r.charger_ok;
        starved |= r.docked && r.ibat_ma < 100;
        done |= r.chg_state == CHG_DONE;
        const int pct = pack_percent(r.vbat_mv, r.docked ? 0 : r.light_ma);
        if (!r.docked) { if (pct < lo_pct) lo_pct = pct; if (pct > hi_pct) hi_pct = pct; }
        if (r.die_c10 > hi_die) hi_die = r.die_c10;
        if (r.cell_c10 > hi_cell) hi_cell = r.cell_c10;
        if (r.cell_c10 < lo_cell) lo_cell = r.cell_c10;
        if (r.light_ma > hi_ma) hi_ma = r.light_ma;
    }
    CHECK(seen_in[INPUT_NONE] && seen_in[INPUT_BATTERY] && seen_in[INPUT_SUPPLY]);
    CHECK(fault && efuse && hot && cold && not_set && starved && done);
    CHECK(lo_pct <= 5 && hi_pct >= 90);
    CHECK(hi_die >= 1050 && hi_cell >= 550 && lo_cell <= 0 && hi_ma >= 7000);   // past the red levels

    // Realistic: a night runs out and recharges, in sensible hours and steps.
    sim_state_t st;
    sim_reset(&st);
    bool saw[3] = { 0 };
    double hours_to_hotel = -1, charged_at = -1, last_pct = st.pack_pct;
    int max_ma = 0, max_die = 0;
    sim_phase_t was = st.phase;
    for (int i = 0; i < 24 * 3600 * 5; i++) {
        sim_reading_t r;
        sim_realistic(&st, 0.2, &r);
        const double h = i * 0.2 / 3600;
        saw[st.phase] = true;
        CHECK(fabs(st.pack_pct - last_pct) < 0.05);     // no jumps
        CHECK(r.vbat_mv > 12000 && r.vbat_mv < 17300);
        last_pct = st.pack_pct;
        if (r.light_ma > max_ma) max_ma = r.light_ma;
        if (r.die_c10 > max_die) max_die = r.die_c10;
        if (st.phase == SIM_AT_HOTEL && hours_to_hotel < 0) hours_to_hotel = h;
        if (was == SIM_AT_HOTEL && st.phase == SIM_ON_PACK && charged_at < 0) charged_at = h;
        was = st.phase;
    }
    CHECK(saw[SIM_ON_PACK] && saw[SIM_ON_EXTERNAL] && saw[SIM_AT_HOTEL]);
    printf("realistic: hotel after %.1f h, charged after %.1f h, peak %d mA, die %.1f C\n", hours_to_hotel, charged_at, max_ma, max_die / 10.0);
    CHECK(hours_to_hotel > 4 && hours_to_hotel < 12);    // a night
    CHECK(charged_at > hours_to_hotel + 1 && charged_at < hours_to_hotel + 8);
    CHECK(max_ma < 5000 && max_die < 850);               // nothing alarming in normal use
}

static void test_charger_limit(void)
{
    CHECK(charger_input_limit_ma(INPUT_NONE, 7000) == CHARGER_PIN_MA);     // not on a supply: untouched
    CHECK(charger_input_limit_ma(INPUT_BATTERY, 7000) == CHARGER_PIN_MA);
    CHECK(charger_input_limit_ma(INPUT_SUPPLY, 0) == CHARGER_PIN_MA);      // the pin's limit is the top
    CHECK(charger_input_limit_ma(INPUT_SUPPLY, 2000) == CHARGER_PIN_MA);
    CHECK(charger_input_limit_ma(INPUT_SUPPLY, 3000) == 1600);             // lights + charger + margin = 5 A
    CHECK(charger_input_limit_ma(INPUT_SUPPLY, 4507) == 90 + 10);          // the chip's lowest setting
    CHECK(charger_input_limit_ma(INPUT_SUPPLY, 7000) == CHARGER_MIN_MA);   // lights alone over: as little as it can
    CHECK(charger_input_limit_ma(INPUT_SUPPLY, -50) == CHARGER_PIN_MA);
    CHECK(charger_input_limit_ma(INPUT_SUPPLY, 3333) % 10 == 0);           // the register's steps
}

static bool has_act(ctl_act_type_t t, int value)
{
    for (int i = 0; i < nacts; i++) if (acts[i].type == t && acts[i].value == value) return true;
    return false;
}

static void test_ab_take(void)
{
    ctl_t c;
    ctl_init(&c, 0);
    ctl_action_t out[CTL_MAX_ACTIONS];
    const ctl_scene_t live = { .body = 1, .eyes = 2, .fins = 0 };
    ctl_set_live(&c, &live, 200, 1);
    ctl_layers_t l = { 6, -1, 0, 0 };
    ctl_set_layers(&c, &l);
    CHECK(ctl_set_ab(&c, true, out) == 0 && c.ab);
    // Without a difference nothing's owed to the Standby brain.
    ctl_layers_t rl = { 6, -1, 0, 0 };
    ctl_scene_t rep = { .body = 1, .eyes = 0, .fins = 0 };
    CHECK(ctl_standby_sync(&c, &rep, &rl, out) == 0);

    // Cueing a preset (LATCH first, then a number) goes to the Standby brain, not Live.
    press(&c, KEY_LATCH);
    press(&c, KEY_NUM3);
    release(&c, KEY_NUM3);
    release(&c, KEY_LATCH);
    CHECK(c.standby.body == 3 && c.live.body == 1);
    int n = ctl_standby_sync(&c, &rep, &rl, out);
    CHECK(n == 1 && out[0].type == ACT_PRESET && out[0].target == TGT_BODY && out[0].value == 3 && out[0].standby);
    rep.body = 3;
    rl.body_pal = 11;                         // the new preset's own palette: leave it
    CHECK(ctl_standby_sync(&c, &rep, &rl, out) == 0);

    // TAKE held: the Standby brain on the LEDs, no preset bumps; back on release.
    press(&c, KEY_TAKE);
    CHECK(has_act(ACT_AB_HOLD, 1) && c.ab_holding);
    for (int i = 0; i < nacts; i++) CHECK(!(acts[i].type == ACT_BUMP && acts[i].target == TGT_BODY));
    release(&c, KEY_TAKE);
    CHECK(has_act(ACT_AB_HOLD, 0) && !c.ab_holding && c.live.body == 1 && c.standby.body == 3);

    // TAKE + LATCH: the controller swaps; they trade places without a preset sent.
    press(&c, KEY_TAKE);
    press(&c, KEY_LATCH);
    CHECK(has_act(ACT_AB_TAKE, 0) && has_act(ACT_AB_HOLD, 0));
    for (int i = 0; i < nacts; i++) CHECK(!(acts[i].type == ACT_PRESET && acts[i].target != TGT_EYES));
    CHECK(c.live.body == 3 && c.standby.body == 1);
    CHECK(c.standby_layers.body_pal == 6);    // Standby: Live as it was
    release(&c, KEY_LATCH);
    CHECK(release(&c, KEY_TAKE) == 0);        // nothing more to do on the way up

    // And the telemetry from the new Live brain agrees; the other reports what it runs.
    const ctl_scene_t live2 = { .body = 3, .eyes = c.live.eyes, .fins = 0 };
    ctl_set_live(&c, &live2, 200, 1);
    ctl_layers_t l2 = { 11, -1, 0, 0 };
    ctl_set_layers(&c, &l2);
    rep = (ctl_scene_t){ .body = 1, .eyes = 0, .fins = 0 };
    rl = (ctl_layers_t){ 6, -1, 0, 0 };
    CHECK(ctl_standby_sync(&c, &rep, &rl, out) == 0);

    // A wrong palette on the Standby brain, and a wrong layer, get put right.
    rl.body_pal = 9;
    rl.body_layer = 3;
    n = ctl_standby_sync(&c, &rep, &rl, out);
    CHECK(n == 2 && out[0].type == ACT_SLOT_PALETTE && out[0].value == 6 && out[0].standby &&
          out[1].type == ACT_SLOT_LOOK && out[1].value == 0);

    // A change made on the Standby brain itself (its web page) becomes Standby's cue.
    {
        ctl_standby_seen_t seen = { .have = false };
        ctl_layers_t r = { 6, 0, 0, 0 };
        ctl_scene_t s = { .body = 1, .eyes = 0, .fins = 0 };
        CHECK(!ctl_standby_adopt(&c, &s, &r, &seen) && seen.have);   // the first report only starts the record
        CHECK(!ctl_standby_adopt(&c, &s, &r, &seen));                // nothing changed
        r.body_pal = 9;                                              // its palette changed by itself
        CHECK(ctl_standby_adopt(&c, &s, &r, &seen) && c.standby_layers.body_pal == 9);
        CHECK(ctl_standby_sync(&c, &s, &r, out) == 0);               // and it stays
        s.body = 5;                                                  // a preset picked on its page
        r.body_pal = 2;                                              // with that preset's own palette
        CHECK(ctl_standby_adopt(&c, &s, &r, &seen) && c.standby.body == 5 && c.standby_layers.body_pal == -1);
        CHECK(ctl_standby_sync(&c, &s, &r, out) == 0);
        // A cue from the base still wins: the report hasn't moved, so nothing is taken.
        press(&c, KEY_LATCH);
        press(&c, KEY_NUM2);
        release(&c, KEY_NUM2);
        release(&c, KEY_LATCH);
        CHECK(c.standby.body == 2);
        CHECK(!ctl_standby_adopt(&c, &s, &r, &seen) && c.standby.body == 2);
        n = ctl_standby_sync(&c, &s, &r, out);
        CHECK(n == 1 && out[0].type == ACT_PRESET && out[0].value == 2);
        s.body = 2;                                                  // the brain follows: no change to take
        CHECK(!ctl_standby_adopt(&c, &s, &r, &seen) && c.standby.body == 2);
        // Back to the state the rest of the test expects.
        c.standby.body = 1;
        c.standby_layers.body_pal = 6;
        c.standby_layers.body_layer = -1;
        // "ab take" from USB serial trades places as TAKE + LATCH does, and back.
        const ctl_t before = c;
        n = ctl_ab_take(&c, out);
        CHECK(n >= 1 && out[0].type == ACT_AB_TAKE && c.live.body == 1 && c.standby.body == 3);
        n = ctl_ab_take(&c, out);
        CHECK(n >= 1 && c.live.body == 3 && c.standby.body == 1);
        c = before;
    }

    // Eyes have no second brain: they change with TAKE by command, as ever.
    ctl_t e;
    ctl_init(&e, 0);
    ctl_set_live(&e, &live, 200, 1);
    ctl_set_ab(&e, true, out);
    e.standby.eyes = 7;
    press(&e, KEY_TAKE);
    bool eye_bump = false;
    for (int i = 0; i < nacts; i++) eye_bump |= acts[i].type == ACT_BUMP && acts[i].target == TGT_EYES && acts[i].value == 7;
    CHECK(eye_bump);
    press(&e, KEY_LATCH);
    bool eye_cmd = false;
    for (int i = 0; i < nacts; i++) eye_cmd |= acts[i].type == ACT_PRESET && acts[i].target == TGT_EYES && acts[i].value == 7;
    CHECK(eye_cmd && e.live.eyes == 7 && e.standby.eyes == 2);

    // A brain dies during a hold: let go of everything, then the old way works.
    ctl_t d;
    ctl_init(&d, 0);
    ctl_set_live(&d, &live, 200, 1);
    ctl_set_ab(&d, true, out);
    press(&d, KEY_TAKE);
    n = ctl_set_ab(&d, false, out);
    CHECK(n >= 1 && out[0].type == ACT_AB_HOLD && out[0].value == 0 && !d.take_preview && !d.ab_holding);
    CHECK(ctl_standby_sync(&d, &rep, &rl, out) == 0);    // one brain: nothing to sync
    // Locking during a hold lets go too.
    ctl_t k;
    ctl_init(&k, 0);
    ctl_set_live(&k, &live, 200, 1);
    ctl_set_ab(&k, true, out);
    press(&k, KEY_TAKE);
    k.locked = true;
    release(&k, KEY_TAKE);
    CHECK(has_act(ACT_AB_HOLD, 0));
}

static void test_eye_thumb(void)
{
    enum { S = EYE_THUMB_SIZE };
    static uint16_t a[S * S], b[S * S];
    #define AT(buf, x, y) (buf)[(y) * S + (x)]
    for (int i = 0; i < preset_count; i++) {
        eye_thumb_draw(a, S, &presets[i], 1.0f, false);
        CHECK((AT(a, S / 2, S / 2) >> 11) <= 2 && ((AT(a, S / 2, S / 2) >> 5) & 63) <= 4);   // the pupil: dark (Aurora's wide glow just reaches it)
        CHECK(AT(a, 0, 0) == 0);               // outside the round screen
        int lit = 0;
        for (int k = 0; k < S * S; k++) lit += a[k] != 0;
        CHECK(lit > S * S / 8);
        // Spirals are mirrored on the port eye; rings look the same on both.
        eye_thumb_draw(b, S, &presets[i], 1.0f, true);
        int differ = 0;   // (rounding moves the odd pixel over a bin)
        for (int y = 0; y < S; y++) for (int x = 0; x < S; x++) differ += AT(a, x, y) != AT(b, S - 1 - x, y);
        CHECK(differ < S * S / 50);
        if (presets[i].spiral_arms) CHECK(memcmp(a, b, sizeof(a)) != 0);
    }
    // Lids: open, drowsy and asleep cover more and more.
    int lit[3];
    for (int s = 0; s < 3; s++) {
        eye_thumb_draw(a, S, &presets[0], eye_thumb_lid(s), false);
        lit[s] = 0;
        for (int k = 0; k < S * S; k++) lit[s] += a[k] != 0;
    }
    CHECK(lit[0] > lit[1] && lit[1] > lit[2] && lit[2] > 0);   // asleep still shows the lid line
    eye_thumb_draw(a, S, &presets[0], eye_thumb_lid(3), false);
    eye_thumb_draw(b, S, &presets[0], 1.0f, false);
    CHECK(memcmp(a, b, sizeof(a)) == 0);       // waking: wide open
    eye_thumb_unlit(a, S, 0x18C3);
    int grey = 0;
    for (int k = 0; k < S * S; k++) { CHECK(a[k] == 0 || a[k] == 0x18C3); grey += a[k] != 0; }
    CHECK(grey > 0);
}

static void test_power_math(void)
{
    CHECK(pack_percent(16800, 0) == 100);
    CHECK(pack_percent(12000, 0) == 0);
    const int mid = pack_percent(15200, 0);    // 3.8 V per cell
    CHECK(mid == 55);
    CHECK(pack_percent(15200, 4000) > mid);     // sagging under load reads higher once corrected
    // the LEMO input: nothing, an external 4S battery (<= 16.8 V), a 20 V USB-C supply
    CHECK(input_kind(0) == INPUT_NONE);
    CHECK(input_kind(3000) == INPUT_NONE);
    CHECK(input_kind(12000) == INPUT_BATTERY);
    CHECK(input_kind(16800) == INPUT_BATTERY);
    CHECK(input_kind(20100) == INPUT_SUPPLY);
    CHECK(input_kind(19000) == INPUT_SUPPLY);
    // the charge graph's colours
    CHECK(pack_level(100) == PACK_OK && pack_level(36) == PACK_OK && pack_level(35) == PACK_LOW);
    CHECK(pack_level(21) == PACK_LOW && pack_level(20) == PACK_CRITICAL && pack_level(0) == PACK_CRITICAL);
    // the pole's power stays off only on a USB-C supply
    CHECK(pole_off_stays(true, INPUT_SUPPLY));
    CHECK(!pole_off_stays(false, INPUT_SUPPLY));
    CHECK(!pole_off_stays(true, INPUT_BATTERY) && !pole_off_stays(true, INPUT_NONE));
    // temperature levels
    CHECK(cell_temp_level(290) == TEMP_OK && cell_temp_level(450) == TEMP_WARN && cell_temp_level(550) == TEMP_BAD);
    CHECK(cell_temp_level(40) == TEMP_WARN && cell_temp_level(0) == TEMP_BAD && cell_temp_level(INT16_MIN) == TEMP_OK);
    CHECK(die_temp_level(410) == TEMP_OK && die_temp_level(850) == TEMP_WARN && die_temp_level(1050) == TEMP_BAD);
    // the LEMO's current split: 3 A to the charger and 2 A to the lights = 5 A in, 3/5 green
    power_flow_t f = power_flow(INPUT_SUPPLY, 20000, 3000, 15000, 3500, 2000, 0, false);
    CHECK(f.in_ma == 5000 && f.split_permille == 600 && !f.starved && f.pack_ma == 3500);
    // the base's own draw counts with the lights: 2 W at 20 V is 100 mA
    f = power_flow(INPUT_SUPPLY, 20000, 3000, 15000, 3500, 2000, 2000, false);
    CHECK(f.in_ma == 5100);
    // plugged in but the pack gets almost nothing: starved; full: not starved
    f = power_flow(INPUT_BATTERY, 15800, 50, 15000, 20, 4000, 0, false);
    CHECK(f.starved && f.split_permille < 20);
    CHECK(!power_flow(INPUT_BATTERY, 16700, 0, 16700, 0, 4000, 0, true).starved);
    // an external battery well below the pack: the pack feeds the lights (body diode)
    f = power_flow(INPUT_BATTERY, 13500, 500, 15500, 400, 3000, 0, false);
    CHECK(f.starved && f.in_ma == 500 && f.pack_ma == 400 - 3000);
    // nothing plugged in: the pack feeds the lights and the base
    f = power_flow(INPUT_NONE, 0, 0, 15000, 0, 3000, 1500, false);
    CHECK(f.in_ma == 0 && f.split_permille == -1 && !f.starved && f.pack_ma == -3100);
    CHECK(runtime_minutes(50, 40000) == 60);    // 40 Wh at 40 W
    CHECK(runtime_minutes(50, 100) == -1);
    CHECK(ina226_calibration() == 2560);
    CHECK(ina226_bus_mv(13440) == 16800);
    CHECK(ina226_current_ma(0xFFFF) == -1);
    CHECK(ina226_current_ma(7000) == 7000);
    CHECK(ina226_power_mw(4) == 100);
    CHECK(bq_charge_state(0x60) == CHG_FAST);
    CHECK(bq_charge_state(0xE0) == CHG_DONE);
    CHECK(bq_die_tenths_c(0xFFFE) == -10);      // -1 C
    CHECK(bq_die_tenths_c(70) == 350);
    // At 25 C the NTC is 10k: 10k || 30.1k = 7.506k, TS = 7.506 / (5.23 + 7.506) = 58.9%
    const int t25 = ts_cell_tenths_c(589);
    CHECK(t25 > 240 && t25 < 260);
    CHECK(ts_cell_tenths_c(560) > t25);         // lower TS = hotter (NTC)
    CHECK(ts_cell_tenths_c(0) == INT16_MIN);
}

// Random overlapping presses with LATCH never touched: once every key is up, no
// bump may still be on, and nothing may have been made permanent (the user saw
// keys swapped quickly leave a preset behind as if latched, 2026-11-09).
static uint32_t rng = 12345;
static uint32_t rnd(uint32_t n) { rng = rng * 1103515245u + 12345u; return (rng >> 16) % n; }

static void test_overlapping_keys_never_latch(void)
{
    static const ctl_key_t pool[] = { KEY_NUM1, KEY_NUM2, KEY_NUM3, KEY_NUM4, KEY_NUM9, KEY_FLASH, KEY_BLACKOUT,
                                      KEY_TAKE, KEY_BODY, KEY_EYES, KEY_FINS, KEY_ALL, KEY_MOD_PALETTE, KEY_MOD_LAYER,
                                      (ctl_key_t)(KEY_PRESET1 + 4), (ctl_key_t)(KEY_LAYER1 + 1), (ctl_key_t)(KEY_PALETTE1 + 2) };
    const int npool = sizeof(pool) / sizeof(pool[0]);
    int bad_end = 0, bad_perm = 0, first_fail = -1;
    for (int run = 0; run < 5000; run++) {
        ctl_t c;
        ctl_init(&c, 0);
        live(&c, 1, 1, 1);
        layers(&c, 3, 4, 0, 0);
        c.standby.body = 7; c.standby.eyes = 7; c.standby.fins = 2; c.standby_set = true;
        bool down[KEY_COUNT] = { 0 };
        bool on[8] = { 0 }, perm = false;
        const int steps = 2 + (int)rnd(12);
        for (int i = 0; i < steps * 2; i++) {
            const ctl_key_t k = pool[rnd(npool)];
            const int n = down[k] ? release(&c, k) : press(&c, k);
            down[k] = !down[k];
            for (int j = 0; j < n; j++) {
                const ctl_action_t *a = &acts[j];
                if (a->type == ACT_BUMP) on[a->target & 7] = true;
                if (a->type == ACT_BUMP_END) on[a->target & 7] = false;
                if (a->type == ACT_AB_HOLD) on[7] = a->value;
                if (a->type == ACT_PRESET || a->type == ACT_SLOT_LOOK || a->type == ACT_SLOT_PALETTE || a->type == ACT_AB_TAKE) perm = true;
            }
        }
        for (int k = 0; k < KEY_COUNT; k++)
            if (down[k]) {
                const int n = release(&c, (ctl_key_t)k);
                for (int j = 0; j < n; j++) {
                    const ctl_action_t *a = &acts[j];
                    if (a->type == ACT_BUMP) on[a->target & 7] = true;
                    if (a->type == ACT_BUMP_END) on[a->target & 7] = false;
                    if (a->type == ACT_AB_HOLD) on[7] = a->value;
                    if (a->type == ACT_PRESET || a->type == ACT_SLOT_LOOK || a->type == ACT_SLOT_PALETTE || a->type == ACT_AB_TAKE) perm = true;
                }
            }
        bool any = false;
        for (int s = 0; s < 8; s++) any |= on[s];
        if (any) bad_end++;
        if (perm) bad_perm++;
        if ((any || perm) && first_fail < 0) first_fail = run;
    }
    if (bad_end || bad_perm) printf("overlap: %d runs left a bump on, %d made something permanent (first run %d)\n", bad_end, bad_perm, first_fail);
    CHECK(bad_end == 0);
    CHECK(bad_perm == 0);
}

int main(void)
{
    test_standby_fills_from_live();
    test_number_is_a_bump();
    test_latch_number();
    test_take_is_a_bump_latch_swaps();
    test_take_with_nothing_staged();
    test_latch_first_cues_standby();
    test_targets();
    test_pages_and_brightness();
    test_modifiers_and_own_keys();
    test_palette_scroll();
    test_palettes();
    test_layers();
    test_standby_layers();
    test_fins_and_all();
    test_pole_power_key();
    test_flash_blackout();
    test_lock();
    test_autolock();
    test_leds();
    test_names();
    test_power_math();
    test_log_format();
    test_power_sim();
    test_eye_thumb();
    test_charger_limit();
    test_ab_take();
    test_overlapping_keys_never_latch();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    printf("all base host tests passed\n");
    return 0;
}
