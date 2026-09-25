#include "presets.h"

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
