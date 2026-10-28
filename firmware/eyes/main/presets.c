#include "presets.h"

#include <math.h>
#include <string.h>

// Spiral families: every preset in a family shares everything but its
// palette (and so shares one spiral map).
#define HYPNO_FAMILY                                                          \
    .palette_step = 0.25f, .palette_music = 0.5f,                             \
    .rings = 0, .fill = 0.05f,                                                \
    .pupil_scale = 1.0f, .rim_amp = 1.0f, .rim_white = 0.4f,                  \
    .ripple_pos = 3.0f, .ripple_white = 0.5f,                                 \
    .spiral_arms = 3, .spiral_twist = 110.0f, .spin = 0.3f, .spin_per_beat = 1.0f / 3.0f

#define VORTEX_FAMILY                                                         \
    .palette_step = 0.3f, .palette_music = 0.3f,                              \
    .rings = 0, .fill = 0.1f,                                                 \
    .pupil_scale = 0.9f, .rim_amp = 1.2f, .rim_white = 0.6f,                  \
    .ripple_pos = 2.0f, .ripple_white = 0.6f,                                 \
    .spiral_arms = 5, .spiral_twist = 60.0f, .spin = 0.3f, .spin_per_beat = 1.0f / 3.0f

// Presets play in this order. Spirals are interleaved with ring presets so
// the cycle alternates between the two styles.
const preset_t presets[] = {
    {
        // Rainbow neon rings on black (the original look).
        .name = "Neon",
        .rainbow = true, .hue_spread = 28.0f, .sat = 1.0f,
        .rings = 5, .ring_width = 2.2f, .halo_sigma = 9.0f, .halo_amp = 0.22f,
        .wobble_px = 4.0f,
        .pupil_scale = 1.0f, .rim_amp = 1.0f, .rim_white = 0.75f,
        .ripple_pos = 180.0f, .ripple_white = 0.6f,
    },
    {
        // Hypnotic spiral: three glowing arms twisting out from the pupil, spinning in
        // time with the music (an arm per beat), twice as fast in hype. Violet, pink, cyan.
        .name = "Hypno",
        .palette = { { 0.6f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.6f }, { 0.0f, 0.8f, 1.0f }, { 0.85f, 0.85f, 1.0f } },
        HYPNO_FAMILY,
    },
    {
        // Fire: soft thick bands in red, orange and gold, flowing outward
        // like heat, with white-hot beat ripples.
        .name = "Inferno",
        .peak_beats = true,  // twitchy: reacts to every sound peak
        .palette = { { 0.55f, 0.0f, 0.0f }, { 1.0f, 0.3f, 0.0f }, { 1.0f, 0.75f, 0.1f }, { 0.9f, 0.15f, 0.0f } },
        .palette_step = 0.2f, .palette_music = 0.5f,
        .rings = 7, .ring_width = 3.5f, .halo_sigma = 14.0f, .halo_amp = 0.35f,
        .fill = 0.12f, .flow = 0.7f, .wobble_px = 7.0f,
        .pupil_scale = 0.9f, .rim_amp = 1.2f, .rim_white = 0.5f,
        .ripple_pos = 2.0f, .ripple_white = 0.7f,
    },
    {
        // Vortex in sunset orange, pink, purple and gold.
        .name = "Vortex Sunset",
        .palette = { { 1.0f, 0.45f, 0.0f }, { 1.0f, 0.3f, 0.55f }, { 0.6f, 0.15f, 0.8f }, { 1.0f, 0.8f, 0.2f } },
        VORTEX_FAMILY,
    },
    {
        // Deep sea shark: a big black pupil in a solid teal-to-navy iris
        // with fine striations, drifting slowly, with a cyan glow.
        .name = "Abyss",
        .palette = { { 0.0f, 0.08f, 0.25f }, { 0.0f, 0.45f, 0.6f }, { 0.2f, 0.9f, 1.0f }, { 0.0f, 0.25f, 0.5f } },
        .palette_step = 0.08f, .palette_music = 0.4f,
        .rings = 14, .ring_width = 0.9f, .halo_sigma = 5.0f, .halo_amp = 0.1f,
        .fill = 0.35f, .flow = 0.15f, .wobble_px = 1.5f,
        .pupil_scale = 1.5f, .rim_amp = 0.7f, .rim_white = 0.3f,
        .ripple_pos = 2.0f, .ripple_white = 0.4f,
    },
    {
        // Hypno in white, pale blue and cyan.
        .name = "Hypno Ice",
        .palette = { { 0.9f, 0.95f, 1.0f }, { 0.45f, 0.7f, 1.0f }, { 0.0f, 0.9f, 1.0f }, { 0.75f, 0.9f, 1.0f } },
        HYPNO_FAMILY,
    },
    {
        // Retro synthwave: a few bold hard-edged bands in pink, purple and
        // cyan, sliding outward.
        .name = "Synthwave",
        .palette = { { 1.0f, 0.08f, 0.55f }, { 0.55f, 0.1f, 1.0f }, { 0.0f, 0.85f, 1.0f }, { 0.9f, 0.0f, 0.9f } },
        .palette_step = 0.25f, .palette_music = 0.5f,
        .rings = 4, .ring_width = 7.0f, .bands = true, .halo_sigma = 10.0f, .halo_amp = 0.25f,
        .flow = 0.35f, .wobble_px = 3.0f,
        .pupil_scale = 1.0f, .rim_amp = 1.0f, .rim_white = 0.2f,
        .ripple_pos = 2.0f, .ripple_white = 0.3f,
    },
    {
        // Vortex in violet, magenta, indigo and pink.
        .name = "Vortex Ultraviolet",
        .palette = { { 0.55f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.8f }, { 0.25f, 0.1f, 0.9f }, { 1.0f, 0.4f, 0.9f } },
        VORTEX_FAMILY,
    },
    {
        // Acid: hard-edged bands in lime, green and yellow pulled inward
        // toward a small pinpoint pupil, wobbling hard.
        .name = "Toxic",
        .peak_beats = true,  // twitchy: reacts to every sound peak
        .palette = { { 0.2f, 1.0f, 0.0f }, { 0.8f, 1.0f, 0.0f }, { 0.0f, 0.6f, 0.1f }, { 0.5f, 1.0f, 0.3f } },
        .palette_step = 0.3f, .palette_music = 0.3f,
        .rings = 6, .ring_width = 3.0f, .bands = true, .halo_sigma = 12.0f, .halo_amp = 0.3f,
        .fill = 0.05f, .flow = -0.8f, .wobble_px = 8.0f,
        .pupil_scale = 0.8f, .rim_amp = 1.3f, .rim_white = 0.3f,
        .ripple_pos = 1.0f, .ripple_white = 0.5f,
    },
    {
        // Hypno in lime, green, yellow and teal.
        .name = "Hypno Acid",
        .peak_beats = true,  // twitchy: reacts to every sound peak
        .palette = { { 0.3f, 1.0f, 0.0f }, { 0.9f, 1.0f, 0.0f }, { 0.0f, 0.8f, 0.3f }, { 0.0f, 0.9f, 0.7f } },
        HYPNO_FAMILY,
    },
    {
        // Northern lights: wide soft billowing glows in green, teal and
        // violet over a strong iris fill, drifting slowly.
        .name = "Aurora",
        .palette = { { 0.1f, 1.0f, 0.5f }, { 0.0f, 0.6f, 0.9f }, { 0.6f, 0.2f, 1.0f }, { 0.2f, 0.9f, 0.7f } },
        .palette_step = 0.18f, .palette_music = 0.6f,
        .rings = 6, .ring_width = 6.0f, .halo_sigma = 22.0f, .halo_amp = 0.4f,
        .fill = 0.25f, .flow = 0.12f, .wobble_px = 10.0f,
        .pupil_scale = 1.1f, .rim_amp = 0.6f, .rim_white = 0.4f,
        .ripple_pos = 2.0f, .ripple_white = 0.3f,
    },
    {
        // Tight five-armed spiral turning as fast as Hypno (a third of a turn per
        // beat). Electric blue and white.
        .name = "Vortex",
        .palette = { { 0.0f, 0.3f, 1.0f }, { 0.4f, 0.8f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 0.2f, 0.1f, 0.9f } },
        VORTEX_FAMILY,
    },
    {
        // Ice: many thin crisp rings in white and pale blue, drifting slowly
        // inward, with white beat ripples.
        .name = "Frost",
        .palette = { { 0.85f, 0.95f, 1.0f }, { 0.5f, 0.75f, 1.0f }, { 1.0f, 1.0f, 1.0f }, { 0.6f, 0.85f, 0.95f } },
        .palette_step = 0.1f, .palette_music = 0.2f,
        .rings = 9, .ring_width = 1.2f, .halo_sigma = 6.0f, .halo_amp = 0.18f,
        .fill = 0.08f, .flow = -0.1f, .wobble_px = 2.0f,
        .pupil_scale = 1.2f, .rim_amp = 1.0f, .rim_white = 0.8f,
        .ripple_pos = 0.0f, .ripple_white = 0.9f,
    },
    {
        // Hypno in red, orange and gold.
        .name = "Hypno Ember",
        .palette = { { 0.8f, 0.0f, 0.0f }, { 1.0f, 0.35f, 0.0f }, { 1.0f, 0.8f, 0.15f }, { 1.0f, 0.15f, 0.05f } },
        HYPNO_FAMILY,
    },
    {
        // The full rainbow spectrum as solid hard-edged stripes, flowing outward.
        .name = "Prism",
        .rainbow = true, .hue_spread = 60.0f, .sat = 1.0f,
        .rings = 6, .ring_width = 5.0f, .bands = true, .halo_sigma = 8.0f, .halo_amp = 0.2f,
        .flow = 0.5f, .wobble_px = 5.0f,
        .pupil_scale = 1.0f, .rim_amp = 1.0f, .rim_white = 0.9f,
        .ripple_pos = 180.0f, .ripple_white = 0.8f,
    },
    {
        // Vortex in emerald, green, lime and gold.
        .name = "Vortex Jungle",
        .palette = { { 0.0f, 0.7f, 0.35f }, { 0.1f, 1.0f, 0.2f }, { 0.6f, 1.0f, 0.1f }, { 1.0f, 0.85f, 0.2f } },
        VORTEX_FAMILY,
    },
    {
        // Hypno in candy pastels: pink, mint, lavender and peach.
        .name = "Hypno Candy",
        .palette = { { 1.0f, 0.45f, 0.75f }, { 0.45f, 1.0f, 0.75f }, { 0.75f, 0.6f, 1.0f }, { 1.0f, 0.75f, 0.5f } },
        HYPNO_FAMILY,
    },
    {
        // Vortex in deep red, orange and yellow.
        .name = "Vortex Magma",
        .peak_beats = true,  // twitchy: reacts to every sound peak
        .palette = { { 0.7f, 0.0f, 0.0f }, { 1.0f, 0.4f, 0.0f }, { 1.0f, 0.9f, 0.2f }, { 0.9f, 0.1f, 0.0f } },
        VORTEX_FAMILY,
    },
};

const int preset_count = sizeof(presets) / sizeof(presets[0]);

float preset_spin_rate(const preset_t *ps, float tempo_bpm, float hype)
{
    const float base = tempo_bpm > 0.0f ? tempo_bpm / 60.0f * ps->spin_per_beat : ps->spin;
    return base * (1.0f + hype);
}

// ------------------------------------------------------------------ the look slots
static preset_t looks[2];
static char look_names[2][NL_NAME_LEN];
static bool look_filled[2];
static int look_source[2];         // the built-in preset whose spiral each look uses
static int look_newest = -1;       // slot of the newest look
static nl_eye_look_t look_last;    // the newest look as received, to spot repeats

bool preset_valid(int index)
{
    if (index >= 0 && index < preset_count) return true;
    return index >= PRESET_LOOK && index < PRESET_ANY && look_filled[index - PRESET_LOOK];
}

const preset_t *preset_get(int index)
{
    if (index >= 0 && index < preset_count) return &presets[index];
    if (preset_valid(index)) return &looks[index - PRESET_LOOK];
    return &presets[0];
}

int preset_look_index(void) { return look_newest < 0 ? -1 : PRESET_LOOK + look_newest; }

int preset_spiral_source(int index)
{
    if (index >= 0 && index < preset_count) return index;
    return preset_valid(index) ? look_source[index - PRESET_LOOK] : 0;
}

// The built-in spiral nearest to arms and twist (-1 for no spiral).
static int nearest_spiral(int arms, float twist)
{
    if (arms <= 0) return -1;
    int best = -1;
    float best_d = 0;
    for (int i = 0; i < preset_count; i++) {
        if (presets[i].spiral_arms <= 0) continue;
        const float d = fabsf((float)(presets[i].spiral_arms - arms)) * 100.0f + fabsf(presets[i].spiral_twist - twist);
        if (best < 0 || d < best_d) { best = i; best_d = d; }
    }
    return best;
}

int preset_set_look(const nl_eye_look_t *l, int showing)
{
    if (look_newest >= 0 && !memcmp(l, &look_last, sizeof(*l))) return -1;
    look_last = *l;
    const int n = showing == PRESET_LOOK ? 1 : 0;   // not the one on screen
    preset_t *p = &looks[n];
    memset(p, 0, sizeof(*p));
    memcpy(look_names[n], l->name, NL_NAME_LEN);
    look_names[n][NL_NAME_LEN - 1] = 0;
    p->name = look_names[n][0] ? look_names[n] : "Look";
    p->peak_beats = l->flags & NL_EYE_LOOK_PEAK_BEATS;
    p->rainbow = l->flags & NL_EYE_LOOK_RAINBOW;
    p->bands = l->flags & NL_EYE_LOOK_BANDS;
    for (int i = 0; i < 4; i++)
        p->palette[i] = (preset_rgb_t){ l->palette[i][0] / 255.0f, l->palette[i][1] / 255.0f, l->palette[i][2] / 255.0f };
    p->hue_spread = l->hue_spread; p->sat = l->sat;
    p->palette_step = l->palette_step; p->palette_music = l->palette_music;
    p->rings = l->rings; p->ring_width = l->ring_width;
    p->halo_sigma = l->halo_sigma; p->halo_amp = l->halo_amp;
    p->fill = l->fill; p->flow = l->flow; p->wobble_px = l->wobble_px;
    p->pupil_scale = l->pupil_scale; p->rim_amp = l->rim_amp; p->rim_white = l->rim_white;
    p->ripple_pos = l->ripple_pos; p->ripple_white = l->ripple_white;
    p->spin = l->spin; p->spin_per_beat = l->spin_per_beat;
    const int src = nearest_spiral(l->spiral_arms, l->spiral_twist);
    look_source[n] = src < 0 ? 0 : src;
    p->spiral_arms = src < 0 ? 0 : presets[src].spiral_arms;   // the shape the eye has a map for
    p->spiral_twist = src < 0 ? 0 : presets[src].spiral_twist;
    look_filled[n] = true;
    look_newest = n;
    return PRESET_LOOK + n;
}

static uint8_t to_u8(float v) { return v <= 0 ? 0 : v >= 1 ? 255 : (uint8_t)lroundf(v * 255.0f); }

void preset_to_look(const preset_t *p, uint8_t id, nl_eye_look_t *out)
{
    memset(out, 0, sizeof(*out));
    out->id = id;
    out->flags = (p->peak_beats ? NL_EYE_LOOK_PEAK_BEATS : 0) | (p->rainbow ? NL_EYE_LOOK_RAINBOW : 0) |
                 (p->bands ? NL_EYE_LOOK_BANDS : 0);
    strncpy(out->name, p->name, NL_NAME_LEN - 1);
    for (int i = 0; i < 4; i++) {
        out->palette[i][0] = to_u8(p->palette[i].r);
        out->palette[i][1] = to_u8(p->palette[i].g);
        out->palette[i][2] = to_u8(p->palette[i].b);
    }
    out->rings = (uint8_t)p->rings; out->spiral_arms = (uint8_t)p->spiral_arms;
    out->hue_spread = p->hue_spread; out->sat = p->sat;
    out->palette_step = p->palette_step; out->palette_music = p->palette_music;
    out->ring_width = p->ring_width; out->halo_sigma = p->halo_sigma; out->halo_amp = p->halo_amp;
    out->fill = p->fill; out->flow = p->flow; out->wobble_px = p->wobble_px;
    out->pupil_scale = p->pupil_scale; out->rim_amp = p->rim_amp; out->rim_white = p->rim_white;
    out->ripple_pos = p->ripple_pos; out->ripple_white = p->ripple_white;
    out->spiral_twist = p->spiral_twist; out->spin = p->spin; out->spin_per_beat = p->spin_per_beat;
}
