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
    c->layers = c->standby_layers = (ctl_layers_t){ -1, -1, -1, -1 };
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

// A cued palette or layer look that would change what shows: one differing
// from Live, or any on a new preset (it would replace the preset's own).
static int cue(int cued, int live, bool fresh) { return cued >= 0 && (fresh || cued != live) ? cued : -1; }
static bool body_fresh(const ctl_t *c) { return c->standby.body > 0 && c->standby.body != c->live.body; }
static bool fins_fresh(const ctl_t *c) { return c->standby.fins >= 0 && c->standby.fins != c->live.fins; }
static bool body_cued(const ctl_t *c)
{
    const bool f = body_fresh(c);
    return cue(c->standby_layers.body_pal, c->layers.body_pal, f) >= 0 || cue(c->standby_layers.body_layer, c->layers.body_layer, f) >= 0;
}
static bool fins_cued(const ctl_t *c)
{
    const bool f = fins_fresh(c);
    return cue(c->standby_layers.fins_pal, c->layers.fins_pal, f) >= 0 || cue(c->standby_layers.fins_layer, c->layers.fins_layer, f) >= 0;
}

uint8_t ctl_standby_diff(const ctl_t *c)
{
    uint8_t d = 0;
    if (body_fresh(c) || body_cued(c)) d |= SLOT_BIT(TGT_BODY);
    if (c->standby.eyes > 0 && c->standby.eyes != c->live.eyes) d |= SLOT_BIT(TGT_EYES);
    if (fins_fresh(c) || fins_cued(c)) d |= SLOT_BIT(TGT_FINS);
    return d;
}

void ctl_standby_layers(const ctl_t *c, ctl_layers_t *out)
{
    const ctl_layers_t *q = &c->standby_layers, *l = &c->layers;
    const bool body_same = c->standby.body == c->live.body, fins_same = c->standby.fins == c->live.fins;
    out->body_pal = q->body_pal >= 0 ? q->body_pal : body_same ? l->body_pal : -1;
    out->body_layer = q->body_layer >= 0 ? q->body_layer : body_same ? l->body_layer : 0;
    out->fins_pal = q->fins_pal >= 0 ? q->fins_pal : fins_same ? l->fins_pal : -1;
    out->fins_layer = q->fins_layer >= 0 ? q->fins_layer : fins_same ? l->fins_layer : 0;
}

// ------------------------------------------------------------------ actions
typedef struct {
    ctl_action_t *a;
    int n;
} out_t;

static void emit(out_t *o, ctl_act_type_t type, uint8_t target, uint8_t bump, int value)
{
    if (o->n < CTL_MAX_ACTIONS) o->a[o->n++] = (ctl_action_t){ type, target, bump, (int16_t)value, 0 };
}

static void bump_on(ctl_t *c, out_t *o, uint8_t slot, uint8_t action, int arg)
{
    emit(o, ACT_BUMP, slot, action, arg);
    c->bump_slots |= SLOT_BIT(slot);
}

// A layer look as a scene bump's layer: 0 = off, a layer preset, -1 = leave it.
static uint8_t scene_layer(int look)
{
    if (look == 0) return NL_SCENE_LAYER_OFF;
    if (look > NL_LAYER_PRESET_BASE && look <= NL_LAYER_PRESET_BASE + NL_LAYER_PRESET_COUNT) return (uint8_t)(look - NL_LAYER_PRESET_BASE);
    return 0;
}

// Standby's body or fins as one bump: its preset (if new), palette and layer look.
static void bump_scene(ctl_t *c, out_t *o, uint8_t slot, int preset, int pal, int look)
{
    bump_on(c, o, slot, NL_BUMP_SCENE, nl_scene_arg((uint8_t)preset, scene_layer(look)));
    if (o->n) o->a[o->n - 1].extra = pal >= 0 ? (uint8_t)(pal + 1) : 0;
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
// What was Live drops into Standby, as for presets: Standby becomes Live as
// it was before (same preset, the old palette or layer look).
static void latch_layered(ctl_t *c, out_t *o)
{
    const int v = c->num_preset;
    const ctl_layers_t old = c->num_layers;
    ctl_layers_t *q = &c->standby_layers;
    if (c->num_page == PAGE_PALETTES) {
        if (on_body(c, c->target)) {
            emit(o, ACT_SLOT_PALETTE, NL_SLOT_BODY, 0, v);
            c->layers.body_pal = v;
            c->standby.body = c->live.body;
            q->body_pal = old.body_pal; q->body_layer = -1;
        }
        if (on_fins(c, c->target)) {
            emit(o, ACT_SLOT_PALETTE, NL_SLOT_FINS, 0, v);
            c->layers.fins_pal = v;
            c->standby.fins = c->live.fins;
            q->fins_pal = old.fins_pal; q->fins_layer = -1;
        }
    } else {
        if (on_body(c, c->target)) {
            const int look = old.body_layer == v ? 0 : v;
            emit(o, ACT_SLOT_LOOK, NL_SLOT_BODY_OVER, 0, look);
            c->layers.body_layer = look;
            c->standby.body = c->live.body;
            q->body_pal = -1; q->body_layer = old.body_layer;
        }
        if (on_fins(c, c->target)) {
            const int look = old.fins_layer == v ? 0 : v;
            emit(o, ACT_SLOT_LOOK, NL_SLOT_FINS_OVER, 0, look);
            c->layers.fins_layer = look;
            c->standby.fins = c->live.fins;
            q->fins_pal = -1; q->fins_layer = old.fins_layer;
        }
    }
    bumps_off(c, o);
    c->num_bump = KEY_NONE;
}
static bool is_target(ctl_key_t k) { return k >= KEY_ALL && k <= KEY_FINS; }

// Number key n (1-8) on the current page, cued in Standby for the target.
// A new preset in Standby comes with its own palette and layers.
static void stage(ctl_t *c, int n)
{
    const int preset = n + c->page * CTL_PAGE_SIZE;
    ctl_layers_t *q = &c->standby_layers;
    const bool body = c->target == TGT_ALL || c->target == TGT_BODY, fins = c->target == TGT_ALL || c->target == TGT_FINS;
    switch (c->target) {
        case TGT_ALL:  c->standby.body = preset; c->standby.eyes = preset; c->standby.fins = 0; break;
        case TGT_BODY: c->standby.body = preset; break;
        case TGT_EYES: c->standby.eyes = preset; break;
        case TGT_FINS: c->standby.fins = preset; break;
    }
    if (body) q->body_pal = q->body_layer = -1;
    if (fins) q->fins_pal = q->fins_layer = -1;
}

// Palette or layer look n cued in Standby for the target: the body's, and the
// fins' when Standby has them on a fin preset (or they're the target). A layer
// look Standby already has is cued off.
static void stage_layered(ctl_t *c, int n)
{
    if (c->target == TGT_EYES) return;
    ctl_layers_t now;
    ctl_standby_layers(c, &now);
    ctl_layers_t *q = &c->standby_layers;
    const bool body = on_body(c, c->target), fins = c->target == TGT_FINS || (c->target == TGT_ALL && c->standby.fins > 0);
    if (c->page == PAGE_PALETTES) {
        const int v = c->palettes[n - 1];
        if (body) q->body_pal = v;
        if (fins) q->fins_pal = v;
    } else {
        const int v = NL_LAYER_PRESET_BASE + n;
        if (body) q->body_layer = now.body_layer == v ? 0 : v;
        if (fins) q->fins_layer = now.fins_layer == v ? 0 : v;
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
        if (old.body > 0) {
            c->standby.body = old.body;
            c->standby_layers.body_pal = c->num_layers.body_pal;
            c->standby_layers.body_layer = c->num_layers.body_layer;
        }
        c->layers.body_pal = -1;   // the new preset's, reported shortly
        c->layers.body_layer = 0;
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
        if (old.fins >= 0) {
            c->standby.fins = old.fins;
            c->standby_layers.fins_pal = c->num_layers.fins_pal;
            c->standby_layers.fins_layer = c->num_layers.fins_layer;
        }
        c->layers.fins_pal = -1;
        c->layers.fins_layer = 0;
    }
    bumps_off(c, o);
    c->num_bump = KEY_NONE;   // its release changes nothing
}

static void take_press(ctl_t *c, out_t *o)
{
    // Show Standby as bumps while TAKE is held; LATCH meanwhile keeps it. A
    // preset alone is a preset bump; with a palette or layer look cued, a
    // scene bump carries them together.
    const uint8_t diff = ctl_standby_diff(c);
    const ctl_layers_t *q = &c->standby_layers, *l = &c->layers;
    c->take_layers = *l;
    if (diff & SLOT_BIT(TGT_BODY)) {
        const bool f = body_fresh(c);
        if (!body_cued(c)) bump_on(c, o, TGT_BODY, NL_BUMP_PRESET, c->standby.body);
        else bump_scene(c, o, TGT_BODY, f ? c->standby.body : 0, cue(q->body_pal, l->body_pal, f), cue(q->body_layer, l->body_layer, f));
    }
    if (diff & SLOT_BIT(TGT_EYES)) bump_on(c, o, TGT_EYES, NL_BUMP_PRESET, c->standby.eyes);
    if (diff & SLOT_BIT(TGT_FINS)) {
        const bool f = fins_fresh(c);
        if (!fins_cued(c)) bump_on(c, o, TGT_FINS, NL_BUMP_PRESET, c->standby.fins);
        else bump_scene(c, o, TGT_FINS, f ? NL_FIN_PRESET_BASE + c->standby.fins : 0, cue(q->fins_pal, l->fins_pal, f),
                        cue(q->fins_layer, l->fins_layer, f));
    }
    c->take_preview = true;
}

// One side (body or fins) of TAKE: its preset if new, then the cued palette
// and layer look; what was Live (preset, palette, layer look) goes to Standby.
static void take_side(ctl_t *c, out_t *o, bool fins)
{
    const bool f = fins ? fins_fresh(c) : body_fresh(c);
    const int sp = fins ? c->standby.fins : c->standby.body, lp = fins ? c->live.fins : c->live.body;
    int16_t *q_pal = fins ? &c->standby_layers.fins_pal : &c->standby_layers.body_pal;
    int16_t *q_lay = fins ? &c->standby_layers.fins_layer : &c->standby_layers.body_layer;
    const int16_t l_pal = fins ? c->take_layers.fins_pal : c->take_layers.body_pal;
    const int16_t l_lay = fins ? c->take_layers.fins_layer : c->take_layers.body_layer;
    const int pal = cue(*q_pal, l_pal, f), look = cue(*q_lay, l_lay, f);
    if (f) emit(o, ACT_PRESET, fins ? TGT_FINS : TGT_BODY, 0, sp);
    if (pal >= 0) emit(o, ACT_SLOT_PALETTE, fins ? NL_SLOT_FINS : NL_SLOT_BODY, 0, pal);
    if (look >= 0) emit(o, ACT_SLOT_LOOK, fins ? NL_SLOT_FINS_OVER : NL_SLOT_BODY_OVER, 0, look);
    // Live now: the new palette and look (a new preset's own palette is reported shortly).
    const int16_t new_pal = pal >= 0 ? pal : f ? -1 : l_pal, new_lay = look >= 0 ? look : f ? 0 : l_lay;
    // Standby: Live as it was.
    *q_pal = l_pal;
    *q_lay = l_lay;
    if (fins) {
        if (lp >= 0) c->standby.fins = (int8_t)lp;
        c->live.fins = (int8_t)sp;
        c->layers.fins_pal = new_pal;
        c->layers.fins_layer = new_lay;
    } else {
        if (lp > 0) c->standby.body = lp;
        c->live.body = sp;
        c->layers.body_pal = new_pal;
        c->layers.body_layer = new_lay;
    }
}

static void take_release(ctl_t *c, out_t *o, bool commit)
{
    if (commit) {
        // Compared with Live as it was before the preview (WLED reports its bumps as live).
        const ctl_layers_t reported = c->layers;
        c->layers = c->take_layers;
        const uint8_t diff = ctl_standby_diff(c);
        c->layers = reported;
        const ctl_scene_t old = c->live;
        // Commands first, bumps released after them (main runs actions in order),
        // so the old preset never shows in between.
        if (diff & SLOT_BIT(TGT_BODY)) take_side(c, o, false);
        if (diff & SLOT_BIT(TGT_EYES)) {
            emit(o, ACT_PRESET, TGT_EYES, 0, c->standby.eyes);
            c->live.eyes = c->standby.eyes;
            if (old.eyes > 0) c->standby.eyes = old.eyes;
        }
        if (diff & SLOT_BIT(TGT_FINS)) take_side(c, o, true);
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
                if (preset_page(c->page)) stage(c, n);
                else stage_layered(c, n);
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
                c->standby_layers = (ctl_layers_t){ -1, -1, -1, -1 };
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
    // Number keys: the target's Live preset (palette, layer look) red, its
    // Standby one green, both amber.
    ctl_layers_t sb;
    ctl_standby_layers(c, &sb);
    for (int n = 1; n <= CTL_PAGE_SIZE && !preset_page(c->page); n++) {
        const bool pal = c->page == PAGE_PALETTES;
        const int v = pal ? c->palettes[n - 1] : NL_LAYER_PRESET_BASE + n;
        const bool live = (on_body(c, c->target) && v == (pal ? c->layers.body_pal : c->layers.body_layer)) ||
                          (c->target == TGT_FINS && v == (pal ? c->layers.fins_pal : c->layers.fins_layer));
        const bool stby = (on_body(c, c->target) && v == (pal ? sb.body_pal : sb.body_layer)) ||
                          (c->target == TGT_FINS && v == (pal ? sb.fins_pal : sb.fins_layer));
        uint32_t col = live && stby ? C_BOTH : live ? C_LIVE : stby ? C_STANDBY : pal ? C_PALETTE : C_LAYER;
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
