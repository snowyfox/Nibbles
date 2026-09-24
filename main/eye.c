#include "eye.h"

#include <math.h>
#include <string.h>

#define RIPPLE_SPEED_PX_S  320.0f
#define RIPPLE_FADE_S      0.5f
#define RIPPLE_END_PX      340.0f
#define THUMP_DECAY_S      0.15f
#define WAKING_WIDE_S      0.4f
#define ASLEEP_LID         0.06f
#define HUE_SMOOTH_S       1.5f

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static float smoothstep(float e0, float e1, float x)
{
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Move toward target with time constant tau.
static float approach(float v, float target, float tau, float dt)
{
    return v + (target - v) * (1.0f - expf(-dt / tau));
}

static float frand(eye_t *e)
{
    e->rng ^= e->rng << 13;
    e->rng ^= e->rng >> 17;
    e->rng ^= e->rng << 5;
    return (e->rng & 0xFFFFFF) / (float)0x1000000;
}

static float wrap_deg(float h)
{
    h = fmodf(h, 360.0f);
    return h < 0.0f ? h + 360.0f : h;
}

// Shortest signed angular difference b - a, in degrees.
static float hue_delta(float a, float b)
{
    float d = wrap_deg(b - a);
    return d > 180.0f ? d - 360.0f : d;
}

const char *eye_state_name(eye_state_t s)
{
    switch (s) {
    case EYE_AWAKE:  return "awake";
    case EYE_DROWSY: return "drowsy";
    case EYE_ASLEEP: return "asleep";
    case EYE_WAKING: return "waking";
    }
    return "?";
}

void eye_init(eye_t *e, uint32_t seed)
{
    memset(e, 0, sizeof(*e));
    e->rng = seed ? seed : 0x9E3779B9u;
    e->state = EYE_AWAKE;
    e->lid_base = 1.0f;
    e->blink_t = -1.0f;
    e->next_blink_s = BLINK_MIN_S;
    e->hue_drift = e->hue = HUE_COOL_DEG;
    e->next_sacc_s = 1.0f;
    e->p.pupil_r = EYE_PUPIL_RADIUS;
    e->p.lid_open = 1.0f;
    e->p.intensity = IDLE_INTENSITY;
}

static void set_state(eye_t *e, eye_state_t s)
{
    e->state = s;
    e->state_t = 0.0f;
}

static float dance_with_music_bonus(const audio_features_t *a, const motion_features_t *m)
{
    float score = m->dance_score;
    if (a->beat_period_s > 0.0f && m->dance_period_s > 0.0f) {
        static const float mult[] = { 0.5f, 1.0f, 2.0f };
        for (int i = 0; i < 3; i++) {
            float p = a->beat_period_s * mult[i];
            if (fabsf(m->dance_period_s - p) < 0.12f * p) {
                score = fminf(1.0f, score * 1.3f);
                break;
            }
        }
    }
    return score;
}

void eye_update(eye_t *e, const audio_features_t *a, const motion_features_t *m, float dt)
{
    eye_params_t *p = &e->p;
    p->time_s += dt;
    e->state_t += dt;

    bool beat = a->beat_count != e->last_beat;
    e->last_beat = a->beat_count;

    // Sleep only in a genuinely quiet room; wake on any clear sound, a steady
    // beat or a bump. Single "beats" are ignored: near-silence produces false ones.
    const bool music = a->beat_period_s > 0.0f;
    const bool quiet = a->avg_db < QUIET_DB && !music;
    e->silent_s = quiet ? e->silent_s + dt : 0.0f;
    e->sound_s = a->level_db > QUIET_DB + WAKE_MARGIN_DB ? e->sound_s + dt : 0.0f;
    bool wake = music || a->avg_db > QUIET_DB || e->sound_s >= WAKE_SOUND_S || m->jolt_g > WAKE_MOTION_G;

    // Sleep / wake state machine.
    switch (e->state) {
    case EYE_AWAKE:
        e->lid_base = approach(e->lid_base, 1.0f, 0.2f, dt);
        if (e->silent_s > SILENCE_SLEEP_S) set_state(e, EYE_DROWSY);
        break;
    case EYE_DROWSY:
        e->lid_base = fmaxf(ASLEEP_LID, e->lid_base - dt / DROWSY_CLOSE_S);
        if (wake) set_state(e, EYE_WAKING);
        else if (e->lid_base <= ASLEEP_LID) set_state(e, EYE_ASLEEP);
        break;
    case EYE_ASLEEP:
        e->lid_base = ASLEEP_LID;
        if (wake) set_state(e, EYE_WAKING);
        break;
    case EYE_WAKING:
        e->lid_base = approach(e->lid_base, 1.15f, 0.06f, dt);
        if (e->state_t > WAKING_WIDE_S) {
            e->blink_t = 0.0f;
            e->silent_s = 0.0f;
            set_state(e, EYE_AWAKE);
        }
        break;
    }
    bool awake = e->state == EYE_AWAKE || e->state == EYE_WAKING;

    // Blinks.
    if (e->state == EYE_AWAKE && e->blink_t < 0.0f) {
        e->next_blink_s -= dt;
        if (e->next_blink_s <= 0.0f) e->blink_t = 0.0f;
    }
    float blink = 1.0f;
    if (e->blink_t >= 0.0f) {
        e->blink_t += dt;
        if (e->blink_t >= BLINK_DURATION_S) {
            e->blink_t = -1.0f;
            e->next_blink_s = BLINK_MIN_S + frand(e) * (BLINK_MAX_S - BLINK_MIN_S);
        } else {
            blink = 1.0f - sinf((float)M_PI * e->blink_t / BLINK_DURATION_S);
        }
    }
    p->lid_open = e->lid_base * blink;

    // Beats: ripple ring and pupil thump.
    if (beat && awake) {
        int slot = 0;
        for (int i = 1; i < EYE_MAX_RIPPLES; i++) {
            if (p->ripples[i].amp < p->ripples[slot].amp) slot = i;
        }
        p->ripples[slot].r = p->pupil_r;
        p->ripples[slot].amp = 1.0f;
        e->thump = 1.0f;
    }
    e->thump *= expf(-dt / THUMP_DECAY_S);
    for (int i = 0; i < EYE_MAX_RIPPLES; i++) {
        eye_ripple_t *r = &p->ripples[i];
        if (r->amp <= 0.0f) continue;
        r->r += RIPPLE_SPEED_PX_S * dt;
        r->amp *= expf(-dt / RIPPLE_FADE_S);
        if (r->r > RIPPLE_END_PX || r->amp < 0.03f) r->amp = 0.0f;
    }

    // Dancing -> hype: rings flow outward (one ring per beat when the tempo is
    // known), colours cycle faster and everything glows and thumps harder.
    e->dance = dance_with_music_bonus(a, m);
    float hype_target = awake ? smoothstep(DANCE_HYPE_SCORE - 0.15f, DANCE_HYPE_SCORE + 0.1f, e->dance) : 0.0f;
    p->hype = approach(p->hype, hype_target, 0.25f, dt);
    float ring_speed = a->beat_period_s > 0.0f ? 1.0f / a->beat_period_s : HYPE_RING_SPEED;
    p->ring_phase = fmodf(p->ring_phase + ring_speed * p->hype * dt, 1.0f);

    // Loudness -> intensity, pupil size and ring wobble. Fast attack, slow release.
    e->loud_smooth = approach(e->loud_smooth, a->loudness, a->loudness > e->loud_smooth ? 0.04f : 0.3f, dt);
    float breath = 0.5f + 0.5f * sinf(p->time_s * 1.3f);
    float target_int;
    if (awake) target_int = IDLE_INTENSITY + 0.05f * breath + (1.0f - IDLE_INTENSITY) * e->loud_smooth +
                            0.2f * e->thump + HYPE_GLOW_BOOST * p->hype;
    else target_int = SLEEP_INTENSITY * (0.6f + 0.4f * breath);
    p->intensity = clampf(approach(p->intensity, target_int, 0.05f, dt), 0.0f, 1.0f);
    p->pupil_r = EYE_PUPIL_RADIUS + EYE_PUPIL_LOUD_GROW * e->loud_smooth + EYE_PUPIL_THUMP * e->thump * (1.0f + p->hype);
    p->wobble = e->loud_smooth * (1.0f + p->hype);

    // Colour: slow drift, pulled toward the music's warm/cool balance.
    e->hue_drift = wrap_deg(e->hue_drift + HUE_DRIFT_DEG_PER_S * (1.0f + HYPE_HUE_BOOST * p->hype) * dt);
    float music_hue = HUE_COOL_DEG + (HUE_WARM_DEG - HUE_COOL_DEG) * a->warmth;
    float pull = HUE_PULL * smoothstep(0.05f, 0.3f, e->loud_smooth);
    float target_hue = wrap_deg(e->hue_drift + pull * hue_delta(e->hue_drift, music_hue));
    e->hue = wrap_deg(e->hue + hue_delta(e->hue, target_hue) * (1.0f - expf(-dt / HUE_SMOOTH_S)));
    p->hue = e->hue;

    // Where the pupil looks: motion spring plus idle saccades.
    float idle = 1.0f - clampf(m->energy_g / DANCE_MIN_ENERGY_G, 0.0f, 1.0f);
    e->next_sacc_s -= dt;
    if (e->next_sacc_s <= 0.0f) {
        e->sacc_tx = (frand(e) - 0.5f) * 0.6f;
        e->sacc_ty = (frand(e) - 0.5f) * 0.4f;
        e->next_sacc_s = 1.5f + frand(e) * 2.5f;
    }
    e->sacc_x = approach(e->sacc_x, e->sacc_tx, 0.04f, dt);
    e->sacc_y = approach(e->sacc_y, e->sacc_ty, 0.04f, dt);
    float lx = m->look_x + idle * e->sacc_x;
    float ly = m->look_y + idle * e->sacc_y;
    if (!awake) { lx *= 0.3f; ly = ly * 0.3f + 0.3f; }
    float r = sqrtf(lx * lx + ly * ly);
    if (r > 1.0f) { lx /= r; ly /= r; }
    p->pupil_x = lx * EYE_MAX_LOOK_PX;
    p->pupil_y = ly * EYE_MAX_LOOK_PX;
}
