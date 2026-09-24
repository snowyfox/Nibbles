// Visual presets: how the eye is drawn. The eye's behaviour (sound, motion,
// beats, hype, blinking, sleep) is shared; presets change palette, ring
// structure, motion and pupil style.
#pragma once

#include <stdbool.h>

typedef struct {
    float r, g, b;
} preset_rgb_t;

typedef struct {
    const char *name;

    // Colour. Rainbow presets colour ring n with hue + n * hue_spread (hue
    // drifts with time and music). Palette presets sample a cyclic 4-colour
    // palette at n * palette_step, shifted by the eye's hue * palette_music.
    bool rainbow;
    float hue_spread, sat;
    preset_rgb_t palette[4];
    float palette_step, palette_music;

    // Rings between the pupil and the iris edge.
    int rings;
    float ring_width;        // glow sigma, or half-width of a hard-edged band
    bool bands;              // hard-edged bands instead of glowing lines
    float halo_sigma, halo_amp;
    float fill;              // soft glow filling the iris
    float flow;              // rings per second drifting outward when calm (negative = inward)
    float wobble_px;         // how far rings wobble with loudness

    // Pupil, ripples and outline.
    float pupil_scale;       // drawn pupil size relative to the eye's pupil radius
    float rim_amp, rim_white;
    float ripple_pos;        // ripple colour: ring position (palette) or hue offset in degrees (rainbow)
    float ripple_white;      // how much ripples are washed toward white
    float outline_amp;

    // Spiral arms (0 = plain rings): the pattern twists and spins.
    int spiral_arms;
    float spiral_twist;      // pixels an arm travels outward per full turn
    float spin;              // turns per second when there is no steady beat
    float spin_per_beat;     // turns per beat while music has a tempo; hype doubles it
} preset_t;

extern const preset_t presets[];
extern const int preset_count;

// Spin speed (turns per second) for a spiral preset: locked to the music's
// tempo when there is one, the preset's calm spin otherwise, doubled by hype.
float preset_spin_rate(const preset_t *ps, float tempo_bpm, float hype);
