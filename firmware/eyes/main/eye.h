// Eye behaviour: turns audio and motion features into what the renderer draws.
// Pure C with no ESP-IDF dependencies so it can be tested on the host.
#pragma once

#include <stdint.h>
#include "audio_analysis.h"
#include "motion_analysis.h"

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
    eye_params_t p;
} eye_t;

void eye_init(eye_t *e, uint32_t seed);
void eye_update(eye_t *e, const audio_features_t *a, const motion_features_t *m, float dt);
const char *eye_state_name(eye_state_t s);

// Ask for a preset change. The eye blinks and sets p.swap_now for one update
// while the lids are shut.
void eye_request_swap(eye_t *e);
