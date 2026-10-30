#include "controls.h"

#include <string.h>
#include "nibbles_link.h"

#define SLOT_BIT(t) (1u << (t))

static const char *const key_names[KEY_COUNT] = {
    "none", "1", "2", "3", "4", "5", "6", "7", "8",
    "all", "body", "eyes", "fins", "page", "latch", "blackout", "finsensor", "take", "flash", "unlock",
};

const char *ctl_key_name(ctl_key_t key) { return key < KEY_COUNT ? key_names[key] : "?"; }

ctl_key_t ctl_key_by_name(const char *name)
{
    for (int k = 1; k < KEY_COUNT; k++)
        if (!strcmp(name, key_names[k])) return (ctl_key_t)k;
    return KEY_NONE;
}

const char *ctl_target_name(ctl_target_t t)
{
    static const char *names[] = { "ALL", "BODY", "EYES", "FINS" };
    return t <= TGT_FINS ? names[t] : "?";
}

const char *ctl_page_name(int page)
{
    static const char *names[] = { "1-8", "9-16", "PALETTE", "LAYER" };
    return page >= 0 && page < PAGE_COUNT ? names[page] : "?";
}

const char *ctl_palette_name(const ctl_t *c, int palette)
{
    static const char *const names[CTL_PAGE_SIZE] = CTL_DEFAULT_PALETTE_NAMES;
    static const uint8_t defaults[CTL_PAGE_SIZE] = CTL_DEFAULT_PALETTES;
    for (int i = 0; i < CTL_PAGE_SIZE; i++)
        if (c->palettes[i] == palette && defaults[i] == palette) return names[i];
    return NULL;
}

void ctl_init(ctl_t *c, uint32_t autolock_ms)
{
    memset(c, 0, sizeof(*c));
    static const uint8_t pals[CTL_PAGE_SIZE] = CTL_DEFAULT_PALETTES;
    memcpy(c->palettes, pals, sizeof(pals));
    c->layers = (ctl_layers_t){ -1, -1, -1, -1 };
    c->live.fins = c->standby.fins = -1;
    c->fin_sensor = -1;
    c->target = c->target_before = TGT_ALL;
    c->autolock_ms = autolock_ms;
}

void ctl_set_live(ctl_t *c, const ctl_scene_t *live, uint8_t brightness, int fin_sensor)
{
    c->live = *live;
    c->brightness = brightness;
    c->fin_sensor = (int8_t)fin_sensor;
    c->have_live = live->body > 0 || live->eyes > 0 || live->fins >= 0;
    // Fill Standby from Live once, field by field as each side is first heard.
    if (c->standby.body == 0) c->standby.body = live->body;
    if (c->standby.eyes == 0) c->standby.eyes = live->eyes;
    if (c->standby.fins < 0) c->standby.fins = live->fins;
    c->standby_set = c->standby.body > 0 && c->standby.eyes > 0;
}

void ctl_set_layers(ctl_t *c, const ctl_layers_t *layers) { c->layers = *layers; }

uint8_t ctl_standby_diff(const ctl_t *c)
{
    uint8_t d = 0;
    if (c->standby.body > 0 && c->standby.body != c->live.body) d |= SLOT_BIT(TGT_BODY);
    if (c->standby.eyes > 0 && c->standby.eyes != c->live.eyes) d |= SLOT_BIT(TGT_EYES);
    if (c->standby.fins >= 0 && c->standby.fins != c->live.fins) d |= SLOT_BIT(TGT_FINS);
    return d;
}

// ------------------------------------------------------------------ actions
typedef struct {
    ctl_action_t *a;
    int n;
} out_t;

static void emit(out_t *o, ctl_act_type_t type, uint8_t target, uint8_t bump, int value)
{
    if (o->n < CTL_MAX_ACTIONS) o->a[o->n++] = (ctl_action_t){ type, target, bump, (int16_t)value };
}

static void bump_on(ctl_t *c, out_t *o, uint8_t slot, uint8_t action, int arg)
{
    emit(o, ACT_BUMP, slot, action, arg);
    c->bump_slots |= SLOT_BIT(slot);
}

static void bumps_off(ctl_t *c, out_t *o)
{
    for (uint8_t slot = TGT_BODY; slot <= TGT_FINS; slot++)
        if (c->bump_slots & SLOT_BIT(slot)) emit(o, ACT_BUMP_END, slot, 0, 0);
    c->bump_slots = 0;
}

// A bump on the target's slots. A preset on ALL is the whole shark on that
// preset, so fins running their own preset show the body's while it's held.
// Flash and blackout on the body cover the fins (every WLED segment); on
// FINS alone they do nothing (fin bumps are fin presets).
static void bump_target(ctl_t *c, out_t *o, ctl_target_t t, uint8_t action, int arg)
{
    if (t == TGT_ALL || t == TGT_BODY) bump_on(c, o, TGT_BODY, action, arg);
    if (t == TGT_ALL || t == TGT_EYES) bump_on(c, o, TGT_EYES, action, arg);
    if (action == NL_BUMP_PRESET && t == TGT_FINS) bump_on(c, o, TGT_FINS, action, arg);
    if (action == NL_BUMP_PRESET && t == TGT_ALL && c->live.fins > 0) bump_on(c, o, TGT_FINS, action, 0);
}

static ctl_target_t key_target(ctl_key_t k)
{
    return k == KEY_BODY ? TGT_BODY : k == KEY_EYES ? TGT_EYES : k == KEY_FINS ? TGT_FINS : TGT_ALL;
}

static bool is_num(ctl_key_t k) { return k >= KEY_NUM1 && k <= KEY_NUM8; }
static bool preset_page(int page) { return page == PAGE_PRESETS || page == PAGE_PRESETS2; }

// Palettes and layers: which of WLED's layers the target means. The body's go
// to the body (the fins show it while they follow it); the fins' to the fins'
// own layers, which only show while the fins run a fin preset (ALL includes them then).
static bool on_body(const ctl_t *c, ctl_target_t t) { (void)c; return t == TGT_ALL || t == TGT_BODY; }
static bool on_fins(const ctl_t *c, ctl_target_t t) { return t == TGT_FINS || (t == TGT_ALL && c->live.fins > 0); }

// A palette (palette page) or a layer look (layer page) while the number is held.
static void bump_layered(ctl_t *c, out_t *o, int page, int value)
{
    const bool pal = page == PAGE_PALETTES;
    const uint8_t action = pal ? NL_BUMP_PALETTE : NL_BUMP_LAYER;
    if (on_body(c, c->target)) bump_on(c, o, TGT_BODY, action, nl_slot_arg(pal ? NL_SLOT_BODY : NL_SLOT_BODY_OVER, (uint8_t)value));
    if (on_fins(c, c->target)) bump_on(c, o, TGT_FINS, action, nl_slot_arg(pal ? NL_SLOT_FINS : NL_SLOT_FINS_OVER, (uint8_t)value));
}

// The held palette or layer look stays (LATCH while it's held). A layer look
// that's already live switches its layer off instead.
static void latch_layered(ctl_t *c, out_t *o)
{
    const int v = c->num_preset;
    if (c->num_page == PAGE_PALETTES) {
        if (on_body(c, c->target)) { emit(o, ACT_SLOT_PALETTE, NL_SLOT_BODY, 0, v); c->layers.body_pal = v; }
        if (on_fins(c, c->target)) { emit(o, ACT_SLOT_PALETTE, NL_SLOT_FINS, 0, v); c->layers.fins_pal = v; }
    } else {
        if (on_body(c, c->target)) {
            const int look = c->num_layers.body_layer == v ? 0 : v;
            emit(o, ACT_SLOT_LOOK, NL_SLOT_BODY_OVER, 0, look);
            c->layers.body_layer = look;
        }
        if (on_fins(c, c->target)) {
            const int look = c->num_layers.fins_layer == v ? 0 : v;
            emit(o, ACT_SLOT_LOOK, NL_SLOT_FINS_OVER, 0, look);
            c->layers.fins_layer = look;
        }
    }
    bumps_off(c, o);
    c->num_bump = KEY_NONE;
}
static bool is_target(ctl_key_t k) { return k >= KEY_ALL && k <= KEY_FINS; }

// Number key n (1-8) on the current page, cued in Standby for the target.
static void stage(ctl_t *c, int n)
{
    const int preset = n + c->page * CTL_PAGE_SIZE;
    switch (c->target) {
        case TGT_ALL:  c->standby.body = preset; c->standby.eyes = preset; c->standby.fins = 0; break;
        case TGT_BODY: c->standby.body = preset; break;
        case TGT_EYES: c->standby.eyes = preset; break;
        case TGT_FINS: c->standby.fins = preset; break;
    }
}

// The held number becomes Live for the target (LATCH tapped while it's held):
// commands first, then the bumps let go, so nothing changes back on release.
// What was Live drops into Standby, one TAKE away.
static void latch_number(ctl_t *c, out_t *o)
{
    if (!preset_page(c->num_page)) { latch_layered(c, o); return; }
    const ctl_scene_t old = c->live;
    const int p = c->num_preset;
    if ((c->target == TGT_ALL || c->target == TGT_BODY) && p != c->live.body) {
        emit(o, ACT_PRESET, TGT_BODY, c->target == TGT_ALL, p);   // ALL: the preset's eye settings too
        c->live.body = p;
        if (old.body > 0) c->standby.body = old.body;
    }
    if ((c->target == TGT_ALL || c->target == TGT_EYES) && p != c->live.eyes) {
        emit(o, ACT_PRESET, TGT_EYES, 0, p);
        c->live.eyes = p;
        if (old.eyes > 0) c->standby.eyes = old.eyes;
    }
    // FINS: that fin preset; ALL: the fins follow the body again
    const int fins = c->target == TGT_FINS ? p : c->target == TGT_ALL ? 0 : -1;
    if (fins >= 0 && fins != c->live.fins) {
        emit(o, ACT_PRESET, TGT_FINS, 0, fins);
        c->live.fins = fins;
        if (old.fins >= 0) c->standby.fins = old.fins;
    }
    bumps_off(c, o);
    c->num_bump = KEY_NONE;   // its release changes nothing
}

static void take_press(ctl_t *c, out_t *o)
{
    // Show Standby as bumps while TAKE is held; LATCH meanwhile keeps it.
    const uint8_t diff = ctl_standby_diff(c);
    if (diff & SLOT_BIT(TGT_BODY)) bump_on(c, o, TGT_BODY, NL_BUMP_PRESET, c->standby.body);
    if (diff & SLOT_BIT(TGT_EYES)) bump_on(c, o, TGT_EYES, NL_BUMP_PRESET, c->standby.eyes);
    if (diff & SLOT_BIT(TGT_FINS)) bump_on(c, o, TGT_FINS, NL_BUMP_PRESET, c->standby.fins);
    c->take_preview = true;
}

static void take_release(ctl_t *c, out_t *o, bool commit)
{
    if (commit) {
        const uint8_t diff = ctl_standby_diff(c);
        const ctl_scene_t old = c->live;
        // Commands first, bumps released after them (main runs actions in order),
        // so the old preset never shows in between.
        if (diff & SLOT_BIT(TGT_BODY)) {
            emit(o, ACT_PRESET, TGT_BODY, 0, c->standby.body);
            c->live.body = c->standby.body;
            if (old.body > 0) c->standby.body = old.body;
        }
        if (diff & SLOT_BIT(TGT_EYES)) {
            emit(o, ACT_PRESET, TGT_EYES, 0, c->standby.eyes);
            c->live.eyes = c->standby.eyes;
            if (old.eyes > 0) c->standby.eyes = old.eyes;
        }
        if (diff & SLOT_BIT(TGT_FINS)) {
            emit(o, ACT_PRESET, TGT_FINS, 0, c->standby.fins);
            c->live.fins = c->standby.fins;
            if (old.fins >= 0) c->standby.fins = old.fins;
        }
    }
    bumps_off(c, o);
    c->take_preview = false;
}

int ctl_key(ctl_t *c, ctl_key_t key, bool down, uint32_t now_ms, ctl_action_t *out)
{
    out_t o = { out, 0 };
    if (key <= KEY_NONE || key >= KEY_COUNT || c->down[key] == down) return 0;
    c->down[key] = down;
    if (down) c->down_ms[key] = now_ms;
    c->last_key_ms = now_ms;

    if (key == KEY_UNLOCK) {
        if (down) c->lock_toggled = false;  // ctl_tick toggles once it's been held long enough
        return 0;
    }
    if (c->locked) {
        // Only releases of keys that were doing something when the lock came on matter.
        if (!down && (c->bump_slots || c->take_preview)) {
            c->take_preview = false;
            c->num_bump = KEY_NONE;
            bumps_off(c, &o);
        }
        return o.n;
    }

    if (is_target(key)) {
        if (down) {
            c->target_before = c->target;
            c->target = key_target(key);
            c->target_key = key;
            c->target_used = false;
        } else if (c->target_key == key) {
            if (c->target_used) c->target = c->target_before;  // held: momentary
            c->target_key = KEY_NONE;
        }
    } else if (is_num(key)) {
        const int n = key - KEY_NUM1 + 1;
        if (down) {
            if (c->target_key != KEY_NONE) c->target_used = true;
            if (c->down[KEY_PAGE]) {                      // brightness in eighths
                c->page_used = true;
                emit(&o, ACT_BRIGHTNESS, TGT_BODY, 0, n * 255 / CTL_PAGE_SIZE);
            } else if (c->down[KEY_LATCH]) {              // LATCH first: cue it in Standby, unseen
                if (preset_page(c->page)) stage(c, n);    // (palettes and layers have no Standby yet)
            } else {                                      // show it while held
                if (c->num_bump != KEY_NONE) bumps_off(c, &o);
                c->num_bump = key;
                c->num_n = n;
                c->num_page = c->page;
                c->num_layers = c->layers;
                if (preset_page(c->page)) {
                    c->num_preset = n + c->page * CTL_PAGE_SIZE;
                    bump_target(c, &o, c->target, NL_BUMP_PRESET, c->num_preset);
                } else {
                    c->num_preset = c->page == PAGE_PALETTES ? c->palettes[n - 1] : NL_LAYER_PRESET_BASE + n;
                    bump_layered(c, &o, c->page, c->num_preset);
                }
            }
        } else if (c->num_bump == key) {
            c->num_bump = KEY_NONE;
            bumps_off(c, &o);
        }
    } else switch (key) {
        case KEY_LATCH:
            if (down && c->num_bump != KEY_NONE) latch_number(c, &o);
            else if (down && c->take_preview) take_release(c, &o, true);
            break;
        case KEY_PAGE:
            if (down) c->page_used = false;
            else if (!c->page_used) c->page = (c->page + 1) % PAGE_COUNT;
            break;
        case KEY_FLASH:
        case KEY_BLACKOUT:
            if (down) bump_target(c, &o, c->target, key == KEY_FLASH ? NL_BUMP_FLASH : NL_BUMP_BLACKOUT, 0);
            else if (!c->take_preview && c->num_bump == KEY_NONE) bumps_off(c, &o);
            break;
        case KEY_FIN_SENSOR:
            if (down) {
                emit(&o, ACT_FINS, TGT_FINS, 0, c->fin_sensor < 0 ? 2 : !c->fin_sensor);
                if (c->fin_sensor >= 0) c->fin_sensor = !c->fin_sensor;
            }
            break;
        case KEY_TAKE:
            if (down && c->down[KEY_LATCH]) {             // LATCH + TAKE: Standby = Live again
                c->standby = c->live;
            } else if (down) {
                take_press(c, &o);
            } else if (c->take_preview) {                 // let go without LATCH: back as it was
                take_release(c, &o, false);
            }
            break;
        default:
            break;
    }
    return o.n;
}

int ctl_tick(ctl_t *c, uint32_t now_ms, ctl_action_t *out)
{
    out_t o = { out, 0 };
    if (c->down[KEY_UNLOCK] && !c->lock_toggled && now_ms - c->down_ms[KEY_UNLOCK] >= CTL_LOCK_HOLD_MS) {
        c->lock_toggled = true;
        c->locked = !c->locked;
        c->last_key_ms = now_ms;
        if (c->locked) {   // nothing stays held under the lock
            c->take_preview = false;
            c->num_bump = KEY_NONE;
            bumps_off(c, &o);
        }
    }
    if (!c->locked && c->autolock_ms && now_ms - c->last_key_ms >= c->autolock_ms && !c->bump_slots) {
        bool any = false;
        for (int k = 1; k < KEY_COUNT; k++) any |= c->down[k];
        if (!any) c->locked = true;
    }
    return o.n;
}

// ------------------------------------------------------------------ LEDs
#define C_OFF      0x000000
#define C_IDLE     0x202020
#define C_PAGE2    0x101838
#define C_STANDBY  0x00e040   // green, like a video switcher's preview
#define C_LIVE     0xff1010   // red, programme
#define C_BOTH     0xffa000
#define C_LOCKED   0x400000
#define C_PALETTE  0x300830   // palette page, idle
#define C_LAYER    0x003020   // layer page, idle

static uint32_t target_color(ctl_target_t t)
{
    static const uint32_t cols[] = { 0xffffff, 0xff40d0, 0x20c8ff, 0xff8000 };
    return cols[t];
}

static uint32_t dim(uint32_t rgb, int pct)
{
    const uint32_t r = (rgb >> 16 & 255) * pct / 100, g = (rgb >> 8 & 255) * pct / 100, b = (rgb & 255) * pct / 100;
    return r << 16 | g << 8 | b;
}

void ctl_led_colors(const ctl_t *c, uint32_t now_ms, uint32_t rgb[KEY_COUNT])
{
    for (int k = 0; k < KEY_COUNT; k++) rgb[k] = C_OFF;
    const bool blink = (now_ms / 400) & 1;
    if (c->locked) {
        rgb[KEY_UNLOCK] = c->down[KEY_UNLOCK] ? 0xff2020 : C_LOCKED;
        return;
    }
    // Number keys: the target's Live preset red, its Standby green, both amber;
    // on the palette and layer pages, the target's live palette or layer red.
    for (int n = 1; n <= CTL_PAGE_SIZE && !preset_page(c->page); n++) {
        const bool pal = c->page == PAGE_PALETTES;
        const int v = pal ? c->palettes[n - 1] : NL_LAYER_PRESET_BASE + n;
        const bool live = (on_body(c, c->target) && v == (pal ? c->layers.body_pal : c->layers.body_layer)) ||
                          (c->target == TGT_FINS && v == (pal ? c->layers.fins_pal : c->layers.fins_layer));
        uint32_t col = live ? C_LIVE : pal ? C_PALETTE : C_LAYER;
        if (c->target == TGT_EYES) col = C_OFF;   // the eyes have no layers
        if (c->down[KEY_NUM1 + n - 1]) col = 0xffffff;
        rgb[KEY_NUM1 + n - 1] = col;
    }
    for (int n = 1; n <= CTL_PAGE_SIZE && preset_page(c->page); n++) {
        const int preset = n + c->page * CTL_PAGE_SIZE;
        int live = 0, stby = 0;
        switch (c->target) {
            case TGT_ALL:  live = preset == c->live.body || preset == c->live.eyes;
                           stby = preset == c->standby.body || preset == c->standby.eyes; break;
            case TGT_BODY: live = preset == c->live.body; stby = preset == c->standby.body; break;
            case TGT_EYES: live = preset == c->live.eyes; stby = preset == c->standby.eyes; break;
            case TGT_FINS: live = preset == c->live.fins; stby = preset == c->standby.fins; break;
        }
        uint32_t col = c->page ? C_PAGE2 : C_IDLE;
        if (live && stby) col = C_BOTH;
        else if (live) col = C_LIVE;
        else if (stby) col = C_STANDBY;
        if (c->down[KEY_NUM1 + n - 1]) col = 0xffffff;
        rgb[KEY_NUM1 + n - 1] = col;
    }
    for (int k = KEY_ALL; k <= KEY_FINS; k++) {
        const ctl_target_t t = key_target((ctl_key_t)k);
        rgb[k] = t == c->target ? target_color(t) : dim(target_color(t), 12);
    }
    static const uint32_t page_cols[PAGE_COUNT] = { C_IDLE, 0x2040ff, 0xc020c0, 0x00c0a0 };
    rgb[KEY_PAGE] = c->down[KEY_PAGE] ? 0xffffff : page_cols[c->page];
    // LATCH: lit while there's something held to keep (a number or TAKE).
    rgb[KEY_LATCH] = c->down[KEY_LATCH] ? 0xffffff : (c->num_bump != KEY_NONE || c->take_preview) ? (blink ? 0xffd000 : 0x403400) : C_IDLE;
    rgb[KEY_BLACKOUT] = c->down[KEY_BLACKOUT] ? 0xff0000 : 0x300000;
    rgb[KEY_FLASH] = c->down[KEY_FLASH] ? 0xffffff : 0x303030;
    rgb[KEY_FIN_SENSOR] = c->fin_sensor == 1 ? 0x00c040 : c->fin_sensor == 0 ? 0x302000 : C_OFF;
    // TAKE: green while Standby differs from Live (blinking), dim when they match.
    rgb[KEY_TAKE] = c->take_preview ? 0xffffff : ctl_standby_diff(c) ? (blink ? C_STANDBY : dim(C_STANDBY, 25)) : C_IDLE;
    rgb[KEY_UNLOCK] = c->down[KEY_UNLOCK] ? 0x40ff40 : 0x002000;
}
