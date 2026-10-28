// The keypad's logic: Live / Standby like a video switcher, targets, pages,
// bumps and the keys' LED colours. Everything is a bump until it's latched:
// hold a number (or TAKE) to show it, tap LATCH while holding to keep it. Plain C with no ESP-IDF calls, so it runs
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
    KEY_PAGE,        // tap: presets 1-8 / 9-16; hold + number: brightness
    KEY_LATCH,       // tap while a number or TAKE is held: keep it (it becomes Live);
                     // held first, then a number: cue that preset in Standby; + TAKE: Standby = Live
    KEY_BLACKOUT,    // bump while held
    KEY_FIN_SENSOR,  // tap: fin LEDs follow the fins, or stay at the modelled angle (live)
    KEY_TAKE,        // held: show Standby while held; + a tap of LATCH: swap Standby and Live
    KEY_FLASH,       // bump while held
    KEY_UNLOCK,      // hold: lock or unlock the keypad
    KEY_COUNT
} ctl_key_t;

typedef enum {
    TGT_ALL = 0,     // body and eyes (and fins, where it applies)
    TGT_BODY,        // WLED
    TGT_EYES,
    TGT_FINS,
} ctl_target_t;

// What the shark shows (Live) or will show (Standby). Presets are 1-based on
// both sides (WLED preset id; eye preset index + 1); 0 = not known / not set.
typedef struct {
    int16_t body;
    int16_t eyes;
    int8_t fins;     // 1 follow the fins, 0 locked at the model angle, -1 not known
} ctl_scene_t;

typedef enum {
    ACT_PRESET,      // target (TGT_BODY or TGT_EYES), value = preset (1-based)
    ACT_FINS,        // value = 0 locked, 1 follow, 2 toggle
    ACT_BRIGHTNESS,  // value = 0..255 (WLED; the eyes follow it)
    ACT_BUMP,        // slot (TGT_BODY or TGT_EYES), bump = NL_BUMP_* action, value = arg
    ACT_BUMP_END,    // slot
} ctl_act_type_t;

typedef struct {
    ctl_act_type_t type;
    uint8_t target;
    uint8_t bump;
    int16_t value;
} ctl_action_t;

#define CTL_MAX_ACTIONS      8
#define CTL_LOCK_HOLD_MS     1000    // UNLOCK held this long locks or unlocks
#define CTL_PAGE_SIZE        8

typedef struct {
    ctl_scene_t live, standby;
    bool have_live, standby_set;     // standby_set: Standby has been filled from Live once
    uint8_t brightness;              // WLED's, from telemetry
    ctl_target_t target;             // latched target
    ctl_target_t target_before;      // to go back to after a held target key
    ctl_key_t target_key;            // target key being held, or KEY_NONE
    bool target_used;                // a number was pressed while it was held: momentary
    int page;                        // 0: presets 1-8, 1: 9-16
    bool page_used;                  // a number was pressed while PAGE was held: brightness, no page flip
    bool locked;
    bool lock_toggled;               // this UNLOCK press already toggled
    uint32_t autolock_ms;            // lock after this long without a key (0: never)
    uint32_t last_key_ms;
    bool down[KEY_COUNT];
    uint32_t down_ms[KEY_COUNT];
    ctl_key_t num_bump;              // number key held (bumping its preset), or KEY_NONE
    int num_n, num_preset;           // its number (1-8) and preset (with the page) when pressed
    bool take_preview;               // TAKE is down and the Standby bumps are on
    uint8_t bump_slots;              // bit per slot (1 << TGT_BODY, 1 << TGT_EYES) with a bump on
} ctl_t;

void ctl_init(ctl_t *c, uint32_t autolock_ms);

// Telemetry: what the shark shows now (fields 0 / -1 where not known yet).
// The first complete picture also fills Standby.
void ctl_set_live(ctl_t *c, const ctl_scene_t *live, uint8_t brightness);

// A key went down or up. Returns how many actions it wrote to out (at most CTL_MAX_ACTIONS).
int ctl_key(ctl_t *c, ctl_key_t key, bool down, uint32_t now_ms, ctl_action_t *out);

// Call every ~10-50 ms: locking by holding UNLOCK, auto-lock.
int ctl_tick(ctl_t *c, uint32_t now_ms, ctl_action_t *out);

// Which parts of Standby differ from Live (bit per target: 1 << TGT_BODY etc.).
uint8_t ctl_standby_diff(const ctl_t *c);

// Each key's LED colour now (0xRRGGBB, full scale; the LED driver dims it).
void ctl_led_colors(const ctl_t *c, uint32_t now_ms, uint32_t rgb[KEY_COUNT]);

const char *ctl_key_name(ctl_key_t key);
ctl_key_t ctl_key_by_name(const char *name);
const char *ctl_target_name(ctl_target_t t);
