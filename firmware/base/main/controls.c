#include "controls.h"

#include <string.h>
#include "nibbles_link.h"
#include "wled_palettes.h"

#define SLOT_BIT(t) (1u << (t))

// Also read by tools/midi_keys/server.py (the names in quotes, in order).
static const char *const key_names[] = {
    "none", "1", "2", "3", "4", "5", "6", "7", "8",
    "all", "body", "eyes", "fins", "page", "latch", "blackout", "finsensor", "take", "flash", "unlock",
    "9", "10", "11", "12", "13", "14", "15", "16",
    "palette", "layer", "bright",
    "preset1", "preset2", "preset3", "preset4", "preset5", "preset6", "preset7", "preset8",
    "preset9", "preset10", "preset11", "preset12", "preset13", "preset14", "preset15", "preset16",
    "palette1", "palette2", "palette3", "palette4", "palette5", "palette6", "palette7", "palette8",
    "layer1", "layer2", "layer3", "layer4", "layer5", "layer6", "layer7", "layer8",
    "layer9", "layer10", "layer11", "layer12", "layer13", "layer14", "layer15", "layer16",
    "bright1", "bright2", "bright3", "bright4", "bright5", "bright6", "bright7", "bright8",
    "polepower",
    "left", "right", "up", "down",
    "pageup", "pagedown",
};
_Static_assert(sizeof(key_names) / sizeof(key_names[0]) == KEY_COUNT, "a name for every key");

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
    static const char *names[] = { "PRESET", "PALETTE", "LAYER", "BRIGHT" };
    return page >= 0 && page <= PAGE_BRIGHT ? names[page] : "?";
}

const char *ctl_palette_name(const ctl_t *c, int palette)
{
    (void)c;
    return palette >= 0 && palette < WLED_PALETTE_COUNT ? wled_palettes[palette].name : NULL;
}

int ctl_set_palette(const ctl_t *c, int n)
{
    return n >= 1 && n <= CTL_PALETTE_ROWS ? (c->pal_first + n - 1) % WLED_PALETTE_COUNT : -1;
}

// The viewer's list moves by d palettes, round.
static void pal_scroll(ctl_t *c, int d)
{
    c->pal_first = ((c->pal_first + d) % WLED_PALETTE_COUNT + WLED_PALETTE_COUNT) % WLED_PALETTE_COUNT;
}

void ctl_init(ctl_t *c, uint32_t autolock_ms)
{
    memset(c, 0, sizeof(*c));
    static const uint8_t pals[CTL_PALETTES] = CTL_DEFAULT_PALETTES;
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

// What a number or a key of its own stands for.
// FN_PALETTE is a favourite (PALETTE1-8); FN_WLED_PALETTE any of WLED's, by
// id + 1 (a number in palette mode: the viewer's set).
enum { FN_NONE = 0, FN_PRESET, FN_PALETTE, FN_LAYER, FN_BRIGHT, FN_WLED_PALETTE,
       FN_FLASH, FN_BLACKOUT, FN_TAKE };   // the last three: held bumps only (ctl_t.held)

// ------------------------------------------------------------------ actions
typedef struct {
    ctl_action_t *a;
    int n;
} out_t;

static void emit(out_t *o, ctl_act_type_t type, uint8_t target, uint8_t bump, int value)
{
    if (o->n < CTL_MAX_ACTIONS) o->a[o->n++] = (ctl_action_t){ .type = type, .target = target, .bump = bump, .value = (int16_t)value };
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
    if (c->ab_holding) emit(o, ACT_AB_HOLD, 0, 0, 0);
    c->ab_holding = false;
}

// The held number on top (what LATCH keeps), from the held bumps.
static void held_sync(ctl_t *c)
{
    c->num_bump = KEY_NONE;
    for (int i = c->nheld - 1; i >= 0; i--) {
        const int k = c->held[i].kind;
        if (k == FN_FLASH || k == FN_BLACKOUT || k == FN_TAKE) continue;
        c->num_bump = c->held[i].key;
        c->num_kind = k;
        c->num_preset = c->held[i].value;
        c->num_target = (ctl_target_t)c->held[i].target;
        break;
    }
    c->num_layers = c->held_layers;
}

// Nothing held does anything any more (LATCH, the lock): their releases change nothing.
static void held_clear(ctl_t *c)
{
    c->nheld = 0;
    held_sync(c);
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

typedef struct {
    int kind;
    int n;           // 1-based: preset, favourite palette, layer look, brightness eighth, WLED palette id + 1
} fn_t;
static bool is_pal(int kind) { return kind == FN_PALETTE || kind == FN_WLED_PALETTE; }

// The number key's number (1-16), or 0.
static int num_of(ctl_key_t k)
{
    if (k >= KEY_NUM1 && k <= KEY_NUM8) return k - KEY_NUM1 + 1;
    if (k >= KEY_NUM9 && k <= KEY_NUM16) return k - KEY_NUM9 + 9;
    return 0;
}
static ctl_key_t num_key(int n) { return n <= 8 ? (ctl_key_t)(KEY_NUM1 + n - 1) : (ctl_key_t)(KEY_NUM9 + n - 9); }

int ctl_mode(const ctl_t *c)
{
    if (c->down[KEY_MOD_BRIGHT]) return PAGE_BRIGHT;
    if (c->down[KEY_MOD_PALETTE]) return PAGE_PALETTES;
    if (c->down[KEY_MOD_LAYER]) return PAGE_LAYERS;
    return c->page;
}

static fn_t fn_make(int kind, int n)
{
    const int max = kind == FN_PRESET ? CTL_PRESETS : kind == FN_PALETTE ? CTL_PALETTES : kind == FN_LAYER ? CTL_LAYERS
                  : kind == FN_WLED_PALETTE ? WLED_PALETTE_COUNT : CTL_BRIGHTS;
    return n >= 1 && n <= max ? (fn_t){ kind, n } : (fn_t){ FN_NONE, 0 };
}

// Number n as things stand: PAGE held is brightness (as ever), else the mode.
static fn_t number_fn(const ctl_t *c, int n)
{
    const int mode = c->down[KEY_PAGE] ? PAGE_BRIGHT : ctl_mode(c);
    if (mode == PAGE_PALETTES) {
        const int id = ctl_set_palette(c, n);
        return id < 0 ? (fn_t){ FN_NONE, 0 } : fn_make(FN_WLED_PALETTE, id + 1);
    }
    return fn_make(mode == PAGE_LAYERS ? FN_LAYER : mode == PAGE_BRIGHT ? FN_BRIGHT : FN_PRESET, n);
}

// A key of its own (PRESET3, LAYER12, ...), whatever is held.
static fn_t key_fn(ctl_key_t k)
{
    if (k >= KEY_PRESET1 && k <= KEY_PRESET_LAST) return fn_make(FN_PRESET, k - KEY_PRESET1 + 1);
    if (k >= KEY_PALETTE1 && k <= KEY_PALETTE_LAST) return fn_make(FN_PALETTE, k - KEY_PALETTE1 + 1);
    if (k >= KEY_LAYER1 && k <= KEY_LAYER_LAST) return fn_make(FN_LAYER, k - KEY_LAYER1 + 1);
    if (k >= KEY_BRIGHT1 && k <= KEY_BRIGHT_LAST) return fn_make(FN_BRIGHT, k - KEY_BRIGHT1 + 1);
    return (fn_t){ FN_NONE, 0 };
}

// What it means to WLED: the preset, the palette, the layer preset, the brightness.
static int fn_value(const ctl_t *c, fn_t f)
{
    switch (f.kind) {
        case FN_PRESET:  return f.n;
        case FN_PALETTE: return c->palettes[f.n - 1];
        case FN_WLED_PALETTE: return f.n - 1;
        case FN_LAYER:   return NL_LAYER_PRESET_BASE + f.n;
        case FN_BRIGHT:  return f.n * 255 / CTL_BRIGHTS;
        default:         return 0;
    }
}

// Palettes and layers: which of WLED's layers the target means. The body's go
// to the body (the fins show it while they follow it); the fins' to the fins'
// own layers, which only show while the fins run a fin preset (ALL includes them then).
static bool on_body(const ctl_t *c, ctl_target_t t) { (void)c; return t == TGT_ALL || t == TGT_BODY; }
static bool on_fins(const ctl_t *c, ctl_target_t t) { return t == TGT_FINS || (t == TGT_ALL && c->live.fins > 0); }

// A palette or a layer look while its key is held.
static void bump_layered(ctl_t *c, out_t *o, bool pal, int value)
{
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
    if (is_pal(c->num_kind)) {
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
    held_clear(c);
}
static bool is_target(ctl_key_t k) { return k >= KEY_ALL && k <= KEY_FINS; }

// A preset cued in Standby for the target.
// A new preset in Standby comes with its own palette and layers.
static void stage(ctl_t *c, int preset)
{
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
static void stage_layered(ctl_t *c, fn_t f)
{
    if (c->target == TGT_EYES) return;
    ctl_layers_t now;
    ctl_standby_layers(c, &now);
    ctl_layers_t *q = &c->standby_layers;
    const bool body = on_body(c, c->target), fins = c->target == TGT_FINS || (c->target == TGT_ALL && c->standby.fins > 0);
    const int v = fn_value(c, f);
    if (is_pal(f.kind)) {
        if (body) q->body_pal = v;
        if (fins) q->fins_pal = v;
    } else {
        if (body) q->body_layer = now.body_layer == v ? 0 : v;
        if (fins) q->fins_layer = now.fins_layer == v ? 0 : v;
    }
}

// The held number becomes Live for the target (LATCH tapped while it's held):
// commands first, then the bumps let go, so nothing changes back on release.
// What was Live drops into Standby, one TAKE away.
static void latch_number(ctl_t *c, out_t *o)
{
    if (c->num_kind != FN_PRESET) { latch_layered(c, o); return; }
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
    held_clear(c);   // releases change nothing now
}

// Show Standby as bumps while TAKE is held; LATCH meanwhile keeps it. A
// preset alone is a preset bump; with a palette or layer look cued, a
// scene bump carries them together. Compared with the layers from before
// TAKE went down (take_layers), as WLED reports bumps as live.
static void take_show(ctl_t *c, out_t *o)
{
    const ctl_layers_t now_layers = c->layers;
    c->layers = c->take_layers;
    const uint8_t diff = ctl_standby_diff(c);
    c->layers = now_layers;
    const ctl_layers_t *q = &c->standby_layers, *l = &c->take_layers;
    if (c->ab) {   // Two brains: Standby's body and fins are already running; just show them.
        emit(o, ACT_AB_HOLD, 0, 0, 1);
        c->ab_holding = true;
        if (diff & SLOT_BIT(TGT_EYES)) bump_on(c, o, TGT_EYES, NL_BUMP_PRESET, c->standby.eyes);
        return;
    }
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

// Two brains: the controller swaps them. Each keeps what it runs, so Standby is
// simply Live as it was.
static void take_ab(ctl_t *c, out_t *o)
{
    ctl_layers_t now_standby;
    ctl_standby_layers(c, &now_standby);
    const ctl_scene_t old = c->live;
    emit(o, ACT_AB_TAKE, 0, 0, 0);
    if (c->standby.eyes > 0 && c->standby.eyes != old.eyes) {
        emit(o, ACT_PRESET, TGT_EYES, 0, c->standby.eyes);
        c->live.eyes = c->standby.eyes;
        if (old.eyes > 0) c->standby.eyes = old.eyes;
    }
    c->live.body = c->standby.body;
    c->live.fins = c->standby.fins;
    if (old.body > 0) c->standby.body = old.body;
    if (old.fins >= 0) c->standby.fins = old.fins;
    c->standby_layers = c->layers;
    c->layers = now_standby;
}

int ctl_ab_take(ctl_t *c, ctl_action_t *out)
{
    out_t o = { out, 0 };
    if (c->ab) take_ab(c, &o);
    return o.n;
}

static void take_release(ctl_t *c, out_t *o, bool commit)
{
    if (commit && c->ab) {
        take_ab(c, o);
    } else if (commit) {
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

// ------------------------------------------------------------------ held bumps
// Held bump i shows, on the target it was pressed on, replacing what showed:
// its bumps first, then the slots it doesn't use let go, so Live never shows
// in between.
static void show_held(ctl_t *c, out_t *o, int i)
{
    const uint8_t old = c->bump_slots;
    const bool was_ab = c->ab_holding;
    c->bump_slots = 0;
    c->ab_holding = false;
    const ctl_target_t keep = c->target;
    c->target = (ctl_target_t)c->held[i].target;
    const int k = c->held[i].kind, v = c->held[i].value;
    if (k == FN_TAKE) take_show(c, o);
    else if (k == FN_FLASH || k == FN_BLACKOUT) bump_target(c, o, c->target, k == FN_FLASH ? NL_BUMP_FLASH : NL_BUMP_BLACKOUT, 0);
    else if (k == FN_PRESET) bump_target(c, o, c->target, NL_BUMP_PRESET, v);
    else bump_layered(c, o, is_pal(k), v);
    c->target = keep;
    for (uint8_t slot = TGT_BODY; slot <= TGT_FINS; slot++)
        if (old & ~c->bump_slots & SLOT_BIT(slot)) emit(o, ACT_BUMP_END, slot, 0, 0);
    if (was_ab && !c->ab_holding) emit(o, ACT_AB_HOLD, 0, 0, 0);
}

// A bump key went down: on top of the others held, and it shows.
static void held_push(ctl_t *c, out_t *o, ctl_key_t key, int kind, int value)
{
    if (c->nheld == 0) c->held_layers = c->layers;
    if (c->nheld == CTL_BUMP_STACK) {   // the oldest drops off
        memmove(&c->held[0], &c->held[1], sizeof(c->held[0]) * (CTL_BUMP_STACK - 1));
        c->nheld--;
    }
    c->held[c->nheld].key = key;
    c->held[c->nheld].kind = (int8_t)kind;
    c->held[c->nheld].target = (int8_t)c->target;
    c->held[c->nheld].value = (int16_t)value;
    c->nheld++;
    held_sync(c);
    show_held(c, o, c->nheld - 1);
}

// A bump key went up: the one under it shows if it was on top, else it just
// leaves; the last one up goes back to how it was.
static void held_release(ctl_t *c, out_t *o, ctl_key_t key)
{
    int i = c->nheld - 1;
    while (i >= 0 && c->held[i].key != key) i--;
    if (i < 0) return;   // not a bump (cued with LATCH held, or LATCH kept it)
    const bool top = i == c->nheld - 1;
    memmove(&c->held[i], &c->held[i + 1], sizeof(c->held[0]) * (size_t)(c->nheld - 1 - i));
    c->nheld--;
    held_sync(c);
    if (!top) return;
    if (c->nheld) show_held(c, o, c->nheld - 1);
    else bumps_off(c, o);
}

// LATCH: keep the newest held number or TAKE (FLASH and BLACKOUT can't be kept).
static void latch_held(ctl_t *c, out_t *o)
{
    for (int i = c->nheld - 1; i >= 0; i--) {
        const int k = c->held[i].kind;
        if (k == FN_FLASH || k == FN_BLACKOUT) continue;
        if (k == FN_TAKE) {
            take_release(c, o, true);
        } else {
            const ctl_target_t keep = c->target;
            c->target = c->num_target;   // as it was pressed
            latch_number(c, o);
            c->target = keep;
        }
        held_clear(c);
        return;
    }
}

// A number or a key of its own went down: brightness at once; with LATCH held
// first, cue it in Standby, unseen; else show it while held (LATCH keeps it).
static void fn_press(ctl_t *c, out_t *o, ctl_key_t key, fn_t f)
{
    if (f.kind == FN_NONE) return;   // e.g. PALETTE + 12: there's no palette 12
    if (f.kind == FN_BRIGHT) {
        emit(o, ACT_BRIGHTNESS, TGT_BODY, 0, fn_value(c, f));
        return;
    }
    if (c->down[KEY_LATCH]) {
        if (f.kind == FN_PRESET) stage(c, f.n);
        else stage_layered(c, f);
        return;
    }
    held_push(c, o, key, f.kind, fn_value(c, f));
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
        if (!down && (c->bump_slots || c->take_preview || c->nheld)) {
            c->take_preview = false;
            held_clear(c);
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
    } else if (num_of(key) || key_fn(key).kind != FN_NONE) {
        if (down) {
            if (c->target_key != KEY_NONE) c->target_used = true;
            if (c->down[KEY_PAGE]) c->page_used = true;
            fn_press(c, &o, key, num_of(key) ? number_fn(c, num_of(key)) : key_fn(key));
        } else {
            held_release(c, &o, key);
        }
    } else switch (key) {
        case KEY_LATCH:
            if (down) latch_held(c, &o);
            break;
        case KEY_PAGE:
            if (down) c->page_used = false;
            else if (!c->page_used) c->page = (c->page + 1) % PAGE_COUNT;
            break;
        case KEY_FLASH:
        case KEY_BLACKOUT:
            if (down && key == KEY_BLACKOUT && c->down[KEY_PAGE]) {
                c->page_used = true;
                emit(&o, ACT_POLE_POWER, 0, 0, 0);
            } else if (down) {
                held_push(c, &o, key, key == KEY_FLASH ? FN_FLASH : FN_BLACKOUT, 0);
            } else {
                held_release(c, &o, key);
            }
            break;
        case KEY_POLE_POWER:
            if (down) emit(&o, ACT_POLE_POWER, 0, 0, 0);
            break;
        case KEY_LEFT: case KEY_RIGHT: case KEY_PAGE_UP: case KEY_PAGE_DOWN:   // the palette viewer: a screenful
            if (down && ctl_mode(c) == PAGE_PALETTES)
                pal_scroll(c, key == KEY_LEFT || key == KEY_PAGE_UP ? -CTL_PALETTE_ROWS : CTL_PALETTE_ROWS);
            break;
        case KEY_UP: case KEY_DOWN:                                             // one, then by itself while held
            if (down && ctl_mode(c) == PAGE_PALETTES) {
                pal_scroll(c, key == KEY_UP ? -1 : 1);
                c->scroll_next_ms = now_ms + CTL_SCROLL_DELAY_MS;
            }
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
                c->take_layers = c->nheld ? c->held_layers : c->layers;
                c->take_preview = true;
                held_push(c, &o, key, FN_TAKE, 0);
            } else if (c->take_preview) {                 // let go without LATCH: back as it was (or to the bump under it)
                c->take_preview = false;
                held_release(c, &o, key);
            }
            break;
        default:
            break;
    }
    return o.n;
}

int ctl_set_ab(ctl_t *c, bool on, ctl_action_t *out)
{
    out_t o = { out, 0 };
    if (on == c->ab) return 0;
    if (c->take_preview) {   // whatever TAKE was showing, it can't carry on
        bumps_off(c, &o);
        held_clear(c);
        c->take_preview = false;
    }
    c->ab = on;
    return o.n;
}

static void sync_emit(out_t *o, ctl_act_type_t type, uint8_t target, int value)
{
    emit(o, type, target, 0, value);
    if (o->n) o->a[o->n - 1].standby = true;
}

int ctl_standby_sync(const ctl_t *c, const ctl_scene_t *rep, const ctl_layers_t *rl, ctl_action_t *out)
{
    out_t o = { out, 0 };
    if (!c->ab) return 0;
    ctl_layers_t want;
    ctl_standby_layers(c, &want);
    const bool body_right = c->standby.body <= 0 || rep->body == c->standby.body;
    const bool fins_right = c->standby.fins < 0 || rep->fins == c->standby.fins;
    if (!body_right) sync_emit(&o, ACT_PRESET, TGT_BODY, c->standby.body);
    else {
        if (want.body_pal >= 0 && rl->body_pal >= 0 && rl->body_pal != want.body_pal) sync_emit(&o, ACT_SLOT_PALETTE, NL_SLOT_BODY, want.body_pal);
        if (want.body_layer >= 0 && rl->body_layer >= 0 && rl->body_layer != want.body_layer) sync_emit(&o, ACT_SLOT_LOOK, NL_SLOT_BODY_OVER, want.body_layer);
    }
    if (!fins_right) sync_emit(&o, ACT_PRESET, TGT_FINS, c->standby.fins);
    else if (c->standby.fins > 0) {   // the fins' layers only show on a fin preset of their own
        if (want.fins_pal >= 0 && rl->fins_pal >= 0 && rl->fins_pal != want.fins_pal) sync_emit(&o, ACT_SLOT_PALETTE, NL_SLOT_FINS, want.fins_pal);
        if (want.fins_layer >= 0 && rl->fins_layer >= 0 && rl->fins_layer != want.fins_layer) sync_emit(&o, ACT_SLOT_LOOK, NL_SLOT_FINS_OVER, want.fins_layer);
    }
    return o.n;
}

bool ctl_standby_adopt(ctl_t *c, const ctl_scene_t *rep, const ctl_layers_t *rl, ctl_standby_seen_t *last)
{
    if (!c->ab) { last->have = false; return false; }
    if (!last->have) {
        *last = (ctl_standby_seen_t){ *rep, *rl, true };
        return false;
    }
    bool took = false;
    ctl_layers_t *q = &c->standby_layers;
    // A preset brings its own palette and looks: drop that side's cues.
    if (rep->body > 0 && rep->body != last->scene.body && rep->body != c->standby.body) {
        c->standby.body = rep->body;
        q->body_pal = q->body_layer = -1;
        took = true;
    }
    if (rep->fins >= 0 && rep->fins != last->scene.fins && rep->fins != c->standby.fins) {
        c->standby.fins = rep->fins;
        q->fins_pal = q->fins_layer = -1;
        took = true;
    }
    ctl_layers_t want;
    ctl_standby_layers(c, &want);
#define ADOPT(f) \
    if (rl->f >= 0 && last->layers.f >= 0 && rl->f != last->layers.f && want.f >= 0 && rl->f != want.f) { q->f = rl->f; took = true; }
    ADOPT(body_pal) ADOPT(body_layer) ADOPT(fins_pal) ADOPT(fins_layer)
#undef ADOPT
    *last = (ctl_standby_seen_t){ *rep, *rl, true };
    return took;
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
            held_clear(c);
            bumps_off(c, &o);
        }
    }
    // UP or DOWN held (not both) with palettes showing: scroll by itself, faster after a while
    if (!c->locked && ctl_mode(c) == PAGE_PALETTES && c->down[KEY_UP] != c->down[KEY_DOWN]) {
        const ctl_key_t k = c->down[KEY_UP] ? KEY_UP : KEY_DOWN;
        if ((int32_t)(now_ms - c->scroll_next_ms) >= 0) {
            pal_scroll(c, k == KEY_UP ? -1 : 1);
            c->scroll_next_ms = now_ms + (now_ms - c->down_ms[k] >= CTL_SCROLL_FAST_MS ? CTL_SCROLL_FAST_REPEAT_MS : CTL_SCROLL_REPEAT_MS);
        }
        c->last_key_ms = now_ms;   // scrolling counts as using the keys (no auto-lock)
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

// A number's or a key of its own's colour when it isn't pressed.
static uint32_t fn_color(const ctl_t *c, fn_t f, const ctl_layers_t *sb)
{
    if (f.kind == FN_NONE) return C_OFF;
    if (f.kind == FN_BRIGHT) {   // a meter: the step nearest the brightness bright, those below dim
        const int level = (c->brightness * CTL_BRIGHTS + 127) / 255;
        return f.n == level ? 0xffd000 : f.n < level ? 0x302400 : C_IDLE;
    }
    if (f.kind == FN_PRESET) {
        const int preset = f.n;
        bool live = false, stby = false;
        switch (c->target) {
            case TGT_ALL:  live = preset == c->live.body || preset == c->live.eyes;
                           stby = preset == c->standby.body || preset == c->standby.eyes; break;
            case TGT_BODY: live = preset == c->live.body; stby = preset == c->standby.body; break;
            case TGT_EYES: live = preset == c->live.eyes; stby = preset == c->standby.eyes; break;
            case TGT_FINS: live = preset == c->live.fins; stby = preset == c->standby.fins; break;
        }
        return live && stby ? C_BOTH : live ? C_LIVE : stby ? C_STANDBY : preset > 8 ? C_PAGE2 : C_IDLE;
    }
    if (c->target == TGT_EYES) return C_OFF;   // the eyes have no layers
    const bool pal = is_pal(f.kind);
    const int v = fn_value(c, f);
    const bool live = (on_body(c, c->target) && v == (pal ? c->layers.body_pal : c->layers.body_layer)) ||
                      (c->target == TGT_FINS && v == (pal ? c->layers.fins_pal : c->layers.fins_layer));
    const bool stby = (on_body(c, c->target) && v == (pal ? sb->body_pal : sb->body_layer)) ||
                      (c->target == TGT_FINS && v == (pal ? sb->fins_pal : sb->fins_layer));
    return live && stby ? C_BOTH : live ? C_LIVE : stby ? C_STANDBY : pal ? C_PALETTE : C_LAYER;
}

void ctl_led_colors(const ctl_t *c, uint32_t now_ms, uint32_t rgb[KEY_COUNT])
{
    for (int k = 0; k < KEY_COUNT; k++) rgb[k] = C_OFF;
    const bool blink = (now_ms / 400) & 1;
    if (c->locked) {
        rgb[KEY_UNLOCK] = c->down[KEY_UNLOCK] ? 0xff2020 : C_LOCKED;
        return;
    }
    // Numbers and keys of their own: the target's Live preset (palette, layer
    // look) red, its Standby one green, both amber; white while pressed.
    ctl_layers_t sb;
    ctl_standby_layers(c, &sb);
    for (int n = 1; n <= CTL_NUMBERS; n++) {
        const ctl_key_t k = num_key(n);
        rgb[k] = c->down[k] ? 0xffffff : fn_color(c, number_fn(c, n), &sb);
    }
    for (int k = KEY_PRESET1; k <= KEY_BRIGHT_LAST; k++)
        rgb[k] = c->down[k] ? 0xffffff : fn_color(c, key_fn((ctl_key_t)k), &sb);
    rgb[KEY_MOD_PALETTE] = c->down[KEY_MOD_PALETTE] ? 0xffffff : 0x601060;
    rgb[KEY_MOD_LAYER] = c->down[KEY_MOD_LAYER] ? 0xffffff : 0x006050;
    rgb[KEY_MOD_BRIGHT] = c->down[KEY_MOD_BRIGHT] ? 0xffffff : 0x504000;
    rgb[KEY_POLE_POWER] = c->down[KEY_POLE_POWER] ? 0xff0000 : 0x301000;
    for (int k = KEY_LEFT; k <= KEY_PAGE_DOWN; k++)   // lit while they do something: palettes showing
        rgb[k] = c->down[k] ? 0xffffff : ctl_mode(c) == PAGE_PALETTES ? 0x404040 : C_OFF;
    for (int k = KEY_ALL; k <= KEY_FINS; k++) {
        const ctl_target_t t = key_target((ctl_key_t)k);
        rgb[k] = t == c->target ? target_color(t) : dim(target_color(t), 12);
    }
    static const uint32_t page_cols[PAGE_COUNT] = { C_IDLE, 0xc020c0, 0x00c0a0 };
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
