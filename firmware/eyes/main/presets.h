// Visual presets: how the eye is drawn. The eye's behaviour (sound, motion,
// beats, hype, blinking, sleep) is shared; presets change palette, ring
// structure, motion and pupil style.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "nibbles_link.h"

typedef struct {
    float r, g, b;
} preset_rgb_t;

typedef struct {
    const char *name;
    bool peak_beats;         // ripple and thump on every sound peak instead of every beat

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

    // Pupil and ripples (the outline and lid lines are always at full brightness).
    float pupil_scale;       // drawn pupil size relative to the eye's pupil radius
    float rim_amp, rim_white;
    float ripple_pos;        // ripple colour: ring position (palette) or hue offset in degrees (rainbow)
    float ripple_white;      // how much ripples are washed toward white

    // Spiral arms (0 = plain rings): the pattern twists and spins.
    int spiral_arms;
    float spiral_twist;      // pixels an arm travels outward per full turn
    float spin;              // turns per second when there is no steady beat
    float spin_per_beat;     // turns per beat while music has a tempo (1/arms = one arm per beat);
                             // hype doubles it
} preset_t;

extern const preset_t presets[];
extern const int preset_count;

// Looks from WLED presets (NL_MSG_EYE_LOOK) live after the built-in presets,
// in two slots: preset indices PRESET_LOOK and PRESET_LOOK + 1. Each new look
// goes in the slot not on screen, so a new look is a change of preset index
// like any other: the leader blinks to it and the port eye follows its index.
#define PRESET_LOOK preset_count
#define PRESET_ANY  (preset_count + 2)       // indices 0 .. PRESET_ANY - 1
const preset_t *preset_get(int index);       // an empty slot or a bad index: preset 0
bool preset_valid(int index);                // a built-in preset or a filled look slot
int preset_look_index(void);                 // the newest look's index, -1 = none yet
// Take a new look into the slot that isn't `showing` (the index on screen).
// Returns its index, or -1 if it is the newest look already (radio repeats).
int preset_set_look(const nl_eye_look_t *look, int showing);
// The built-in preset whose spiral map a preset uses (looks share one with the
// nearest built-in spiral; their arms and twist are snapped to it).
int preset_spiral_source(int index);
// A preset as a look (the export tool and tests).
void preset_to_look(const preset_t *p, uint8_t id, nl_eye_look_t *out);

// Spin speed (turns per second) for a spiral preset: locked to the music's
// tempo when there is one, the preset's calm spin otherwise, doubled by hype.
float preset_spin_rate(const preset_t *ps, float tempo_bpm, float hype);
