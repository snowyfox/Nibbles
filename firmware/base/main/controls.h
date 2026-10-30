// The keypad's logic: Live / Standby like a video switcher, targets, pages,
// bumps and the keys' LED colours. Everything is a bump until it's latched:
// hold a number (or TAKE) to show it, tap LATCH while holding to keep it.
// PAGE cycles what the numbers are: presets 1-8, presets 9-16, palettes (on
// the target's own layer, the effect keeps running) and layer looks (WLED
// presets NL_LAYER_PRESET_BASE + n on the target's overlay; LATCH on the one
// that's live switches it off). Standby holds palettes and layer looks too:
// LATCH held first, then a number, cues one (a layer look already in Standby
// is cued off); TAKE previews and takes them with Standby's presets. Plain C with no ESP-IDF calls, so it runs
// in the host tests (test/host); main.c feeds it key events and telemetry and
// carries out the actions it returns.
#pragma once

#include <stdbool.h>
#include <stdint.h>

// Logical keys (where each physical key lands is set in config.h, KEYPAD_MAP).
typedef enum {
    KEY_NONE = 0,
    KEY_NUM1, KEY_NUM2, KEY_NUM3, KEY_NUM4, KEY_NUM5, KEY_NUM6, KEY_NUM7, KEY_NUM8,
    KEY_ALL, KEY_BODY, KEY_EYES, KEY_FINS,  // target selectors
    KEY_PAGE,        // tap: next page (presets 1-8, 9-16, palettes, layers); hold + number: brightness
    KEY_LATCH,       // tap while a number or TAKE is held: keep it (it becomes Live);
                     // held first, then a number: cue that preset (palette, layer look) in Standby;
                     // + TAKE: Standby = Live
    KEY_BLACKOUT,    // bump while held
    KEY_FIN_SENSOR,  // tap: fin LEDs follow the fins' real angle, or stay at the modelled angle (live)
    KEY_TAKE,        // held: show Standby while held; + a tap of LATCH: swap Standby and Live
    KEY_FLASH,       // bump while held
    KEY_UNLOCK,      // hold: lock or unlock the keypad
    KEY_COUNT
} ctl_key_t;

typedef enum {
    TGT_ALL = 0,     // the whole shark: body and eyes on one preset, the fins following the body
    TGT_BODY,        // WLED's body segment
    TGT_EYES,
    TGT_FINS,        // WLED's fin segment (fin presets)
} ctl_target_t;

// What the shark shows (Live) or will show (Standby). Presets are 1-based on
// both sides (WLED preset id; eye preset index + 1); 0 = not known / not set.
typedef struct {
    int16_t body;
    int16_t eyes;
    int8_t fins;     // fin preset 1..16 (WLED preset NL_FIN_PRESET_BASE + n), 0 following the body, -1 not known
} ctl_scene_t;

typedef enum {
    ACT_PRESET,      // target (TGT_BODY, TGT_EYES or TGT_FINS), value = preset (1-based; fins: 0 = follow the body);
                     // body: bump = 1 lets the WLED preset's eye settings through (ALL), 0 body only
    ACT_FINS,        // the fins' angle sensor: value = 0 locked at the model angle, 1 follow, 2 toggle
    ACT_BRIGHTNESS,  // value = 0..255 (WLED; the eyes follow it)
    ACT_BUMP,        // slot (TGT_BODY, TGT_EYES or TGT_FINS), bump = NL_BUMP_* action, value = arg
                     // (fin preset bumps: the fin preset; palette and layer bumps: nl_slot_arg(WLED slot, value);
                     // scene bumps: nl_scene_arg(WLED preset id, layer) and extra = palette + 1)
    ACT_BUMP_END,    // slot
    ACT_SLOT_LOOK,   // target = WLED layer slot (nl_slot_t), value = layer preset (0: switch the slot off)
    ACT_SLOT_PALETTE, // target = WLED layer slot, value = palette
} ctl_act_type_t;

typedef struct {
    ctl_act_type_t type;
    uint8_t target;
    uint8_t bump;
    int16_t value;
    uint8_t extra;   // NL_BUMP_SCENE: palette + 1 (0: leave it), the bump's intensity
} ctl_action_t;

#define CTL_MAX_ACTIONS      8
#define CTL_LOCK_HOLD_MS     1000    // UNLOCK held this long locks or unlocks
#define CTL_PAGE_SIZE        8

typedef enum {
    PAGE_PRESETS = 0,    // presets 1-8
    PAGE_PRESETS2,       // presets 9-16
    PAGE_PALETTES,
    PAGE_LAYERS,
    PAGE_COUNT
} ctl_page_t;

// WLED palettes on the palette page's keys 1-8 (main can change them before use).
#define CTL_DEFAULT_PALETTES { 6, 8, 9, 11, 36, 40, 44, 51 }  // Party, Lava, Ocean, Rainbow, Icefire, Magenta, Orange & Teal, Atlantica
#define CTL_DEFAULT_PALETTE_NAMES { "Party", "Lava", "Ocean", "Rainbow", "Icefire", "Magenta", "Orange & Teal", "Atlantica" }

// WLED's layer slots as last reported (NL_MSG_WLED_LAYERS); -1 = not known.
typedef struct {
    int16_t body_pal, fins_pal;      // palette of the body's and the fins' own layer
    int16_t body_layer, fins_layer;  // layer preset showing over the body / fins (0: none)
} ctl_layers_t;

typedef struct {
    ctl_scene_t live, standby;
    ctl_layers_t standby_layers;     // palettes and layer looks cued in Standby (layer 0: off); -1: none cued,
                                     // Standby leaves it as it is, or as its new preset sets it
    bool have_live, standby_set;     // standby_set: Standby has been filled from Live once
    uint8_t brightness;              // WLED's, from telemetry
    int8_t fin_sensor;               // the fins follow their angle sensor: 1, 0, -1 not known
    ctl_target_t target;             // latched target
    ctl_target_t target_before;      // to go back to after a held target key
    ctl_key_t target_key;            // target key being held, or KEY_NONE
    bool target_used;                // a number was pressed while it was held: momentary
    int page;                        // ctl_page_t
    uint8_t palettes[CTL_PAGE_SIZE];  // WLED palette on each key of the palette page
    ctl_layers_t layers;
    bool page_used;                  // a number was pressed while PAGE was held: brightness, no page flip
    bool locked;
    bool lock_toggled;               // this UNLOCK press already toggled
    uint32_t autolock_ms;            // lock after this long without a key (0: never)
    uint32_t last_key_ms;
    bool down[KEY_COUNT];
    uint32_t down_ms[KEY_COUNT];
    ctl_key_t num_bump;              // number key held (bumping its preset), or KEY_NONE
    int num_n, num_preset;           // its number (1-8) and preset (with the page) when pressed
    int num_page;                    // the page it was pressed on
    ctl_layers_t num_layers;         // the layers before it (WLED reports a layer bump as live)
    bool take_preview;               // TAKE is down and the Standby bumps are on
    ctl_layers_t take_layers;        // the layers when TAKE went down (WLED reports its preview as live)
    uint8_t bump_slots;              // bit per slot (1 << TGT_BODY, TGT_EYES, TGT_FINS) with a bump on
} ctl_t;

void ctl_init(ctl_t *c, uint32_t autolock_ms);

// Telemetry: what the shark shows now (fields 0 / -1 where not known yet).
// The first complete picture also fills Standby.
void ctl_set_live(ctl_t *c, const ctl_scene_t *live, uint8_t brightness, int fin_sensor);

// A key went down or up. Returns how many actions it wrote to out (at most CTL_MAX_ACTIONS).
int ctl_key(ctl_t *c, ctl_key_t key, bool down, uint32_t now_ms, ctl_action_t *out);

// Call every ~10-50 ms: locking by holding UNLOCK, auto-lock.
int ctl_tick(ctl_t *c, uint32_t now_ms, ctl_action_t *out);

// WLED's layer slots (from NL_MSG_WLED_LAYERS; fields -1 where not known).
void ctl_set_layers(ctl_t *c, const ctl_layers_t *layers);

// Which parts of Standby differ from Live (bit per target: 1 << TGT_BODY etc.;
// a cued palette or layer look counts for the body or the fins).
uint8_t ctl_standby_diff(const ctl_t *c);

// Standby's palettes and layer looks as TAKE would leave them: the cued one,
// else Live's where Standby keeps Live's preset; else -1 for the palette (the
// new preset's own) and 0 for the layer (a new preset clears it).
void ctl_standby_layers(const ctl_t *c, ctl_layers_t *out);

// Each key's LED colour now (0xRRGGBB, full scale; the LED driver dims it).
void ctl_led_colors(const ctl_t *c, uint32_t now_ms, uint32_t rgb[KEY_COUNT]);

const char *ctl_key_name(ctl_key_t key);
ctl_key_t ctl_key_by_name(const char *name);
const char *ctl_target_name(ctl_target_t t);
const char *ctl_page_name(int page);
// A palette's name if it's one of the palette page's (as set by ctl_init), else NULL.
const char *ctl_palette_name(const ctl_t *c, int palette);
