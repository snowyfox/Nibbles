// Eye behaviour: turns audio and motion features into what the renderer draws.
// Pure C with no ESP-IDF dependencies so it can be tested on the host.
#pragma once

#include <stdint.h>
#include "audio_analysis.h"
#include "motion_analysis.h"
#include "nibbles_link.h"

typedef enum { EYE_AWAKE, EYE_DROWSY, EYE_ASLEEP, EYE_WAKING } eye_state_t;

typedef struct {
    float r, amp;
} eye_ripple_t;

typedef struct {
    float time_s;
    float pupil_x, pupil_y;   // pixels from the display centre
    float pupil_r;            // pixels
    float intensity;          // 0..1 overall glow
    float hue;                // degrees, colour of the innermost ring
    float lid_open;           // 0 closed .. 1 fully open (can overshoot when startled)
    float hype;               // 0..1 how energetic the rings are (from dancing)
    float ring_phase;         // 0..1 how far the rings have flowed outward
    float tempo_bpm;          // current musical tempo, 0 when there is no steady beat
    float wobble;             // 0..1 ring wobble, follows loudness
    eye_ripple_t ripples[EYE_MAX_RIPPLES];
    bool swap_now;            // true for one update: the lids hide the eye, switch preset now
    float gaze_x, gaze_y;     // idle glance part of the pupil offset, -1..1 (shared between eyes)
    bool awake;
    float master;             // 0..1 master level for everything, outline included (blackout bumps)
} eye_params_t;

typedef struct {
    eye_state_t state;
    float state_t;
    uint32_t last_beat;
    uint32_t rng;
    float silent_s, sound_s;
    float lid_base;
    float blink_t, next_blink_s;
    bool swap_pending;        // a preset change is waiting for the lids to hide it
    float swap_t;             // time into the swap blink, -1 when none
    bool swap_done;
    float thump;
    float loud_smooth;
    float hue_drift, hue;
    float sacc_x, sacc_y, sacc_tx, sacc_ty, next_sacc_s;
    float dance;              // dance score incl. music-match bonus
    float activity_peak;      // recent motion activity, held then faded
    float activity_hold_s;
    uint32_t last_peak;
    bool peak_beats;          // react to every sound peak instead of every beat (per preset)
    uint8_t bump;             // nl_bump_action_t being held, 0 = none
    float flash_amt, black_amt;
    eye_params_t p;
} eye_t;

void eye_init(eye_t *e, uint32_t seed);
void eye_update(eye_t *e, const audio_features_t *a, const motion_features_t *m, float dt);
const char *eye_state_name(eye_state_t s);

// Ask for a preset change. The eye blinks and sets p.swap_now for one update
// while the lids are shut.
void eye_request_swap(eye_t *e);

// A bump from the base: NL_BUMP_FLASH startles the eye (full glow, pupil
// snaps small, a ripple), NL_BUMP_BLACKOUT shuts the lids and dims it; 0 ends
// the bump. Preset bumps are handled by the caller.
void eye_set_bump(eye_t *e, uint8_t action);

// Presets choose how the eye reacts to sound: on every beat (false) or on
// every sound peak, several per beat (true): a much busier, twitchier eye.
void eye_set_peak_beats(eye_t *e, bool on);

// Pupil offset from this eye's own motion look plus the shared idle glance.
void eye_compose_pupil(eye_params_t *p, float look_x, float look_y);

// Linked eyes: the leader exports everything both eyes should show alike;
// the other eye applies it, keeping its own motion look. The glance is
// mirrored when the sender is on the other side, so both eyes glance the
// same way in the world.
void eye_export_shared(const eye_t *e, int preset, nl_side_t side, nl_eye_state_t *s);
void eye_apply_shared(eye_t *e, const nl_eye_state_t *s, nl_side_t my_side, const motion_features_t *m);
