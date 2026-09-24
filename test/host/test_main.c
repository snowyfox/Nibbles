// Host tests for the pure-C analysis modules. Run with `make` in this folder.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio_analysis.h"
#include "eye.h"
#include "motion_analysis.h"

static int failures;

#define CHECK(cond, ...) do { \
    if (!(cond)) { failures++; printf("FAIL: "); } else printf("ok:   "); \
    printf(__VA_ARGS__); printf("\n"); } while (0)

static float noise(void) { return (rand() / (float)RAND_MAX) * 2.0f - 1.0f; }

// Kick drum (decaying 60 Hz sine) at `bpm`, hi-hat noise on off-beats, plus background noise.
static void synth_music(float *buf, int n, float bpm, float gain, double *t)
{
    const float period = 60.0f / bpm;
    for (int i = 0; i < n; i++, *t += 1.0 / AUDIO_SAMPLE_RATE) {
        float ph = fmodf((float)*t, period);
        float kick = sinf(2.0f * (float)M_PI * 60.0f * ph) * expf(-ph / 0.08f);
        float hph = fmodf((float)*t + period / 2, period);
        float hat = noise() * expf(-hph / 0.02f) * 0.3f;
        buf[i] = gain * (0.8f * kick + hat + 0.02f * noise());
    }
}

// Levels measured on the board (mic gain removed): silent room -89 dB,
// faint background music -74 to -84 dB.
#define ROOM_AMP      6.2e-5f   // uniform noise at about -89 dB
#define FAINT_MUSIC   0.0005f   // synth_music at about -82 dB, the quiet end of the music

// Simulated microphone: signals are "acoustic" levels referred to 0 dB mic
// gain. The hardware gain lags the analysis' request by two frames (DMA
// buffering) and the ADC clips at full scale.
typedef struct {
    audio_analysis_t a;
    float hpf_hz, hpf_x, hpf_y;  // optional bass roll-off, like a small MEMS mic
    float hw_gain_db, queue[2];
    float peak_db;  // raw peak of the last frame
    int clipped;    // frames that hit full scale
} mic_t;

static void mic_init(mic_t *m)
{
    audio_analysis_init(&m->a, AUDIO_SAMPLE_RATE, MIC_GAIN_START_DB);
    m->hw_gain_db = m->queue[0] = m->queue[1] = MIC_GAIN_START_DB;
    m->clipped = 0;
    m->hpf_hz = m->hpf_x = m->hpf_y = 0.0f;
}

static void mic_feed(mic_t *m, const float *acoustic)
{
    float raw[AUDIO_FRAME], peak = 0.0f;
    const float g = powf(10.0f, m->hw_gain_db / 20.0f);
    bool clip = false;
    const float rc = m->hpf_hz > 0 ? 1.0f / (2.0f * (float)M_PI * m->hpf_hz) : 0.0f;
    const float ha = rc / (rc + 1.0f / AUDIO_SAMPLE_RATE);
    for (int i = 0; i < AUDIO_FRAME; i++) {
        float x = acoustic[i];
        if (m->hpf_hz > 0) {  // one-pole high-pass
            float y = ha * (m->hpf_y + x - m->hpf_x);
            m->hpf_x = x;
            m->hpf_y = y;
            x = y;
        }
        float v = x * g;
        if (v >= 1.0f) { v = 1.0f; clip = true; }
        if (v <= -1.0f) { v = -1.0f; clip = true; }
        raw[i] = v;
        peak = fmaxf(peak, fabsf(v));
    }
    m->clipped += clip;
    m->peak_db = 20.0f * log10f(peak + 1e-9f);
    audio_analysis_process(&m->a, raw);
    m->hw_gain_db = m->queue[0];
    m->queue[0] = m->queue[1];
    m->queue[1] = m->a.out.gain_db;
}

// The tempo the analysis should report for a track at `bpm`.
static float fold_bpm(float bpm)
{
    while (bpm < TEMPO_FOLD_MIN_BPM) bpm *= 2.0f;
    while (bpm > TEMPO_FOLD_MAX_BPM) bpm *= 0.5f;
    return bpm;
}

#define FRAMES(sec) ((int)((sec) * AUDIO_SAMPLE_RATE / AUDIO_FRAME))

// Music at `amp` for `secs`; beats and loudness measured over the last `measure` seconds.
static void test_beats(float bpm, float amp, float secs, float measure, const char *label)
{
    static mic_t m;
    mic_init(&m);
    float buf[AUDIO_FRAME];
    double t = 0;
    const int frames = FRAMES(secs), from = frames - FRAMES(measure);
    uint32_t beats_from = 0;
    float loud_sum = 0.0f;
    for (int f = 0; f < frames; f++) {
        synth_music(buf, AUDIO_FRAME, bpm, amp, &t);
        mic_feed(&m, buf);
        if (f == from) beats_from = m.a.out.beat_count;
        if (f > from) loud_sum += m.a.out.loudness;
    }
    float loud_avg = loud_sum / (frames - from - 1);
    float detected_bpm = (m.a.out.beat_count - beats_from) / measure * 60.0f;
    float period_bpm = m.a.out.beat_period_s > 0 ? 60.0f / m.a.out.beat_period_s : 0;
    // Beats follow strong onsets, so off-beat hi-hats may count too: expect
    // between the kick rate and twice it. The tempo is the musical beat.
    CHECK(detected_bpm > bpm * 0.92f && detected_bpm < bpm * 2.05f, "%s %.0f bpm: %.1f beats/min counted (gain %.0f dB)",
          label, bpm, detected_bpm, m.a.out.gain_db);
    const float want = fold_bpm(bpm);
    CHECK(fabsf(period_bpm - want) < want * 0.08f, "%s %.0f bpm: tempo %.1f bpm (expect %.0f)", label, bpm, period_bpm, want);
    CHECK(loud_avg > 0.3f, "%s %.0f bpm: average loudness %.2f", label, bpm, loud_avg);
}

// A more realistic dance track: kick sweeping 150 -> 50 Hz with a click,
// an off-beat bass note, hi-hats, and a little noise.
static void synth_track(float *buf, int n, float bpm, float amp, double *t)
{
    const float period = 60.0f / bpm;
    for (int i = 0; i < n; i++, *t += 1.0 / AUDIO_SAMPLE_RATE) {
        const float ph = fmodf((float)*t, period);
        const float kick = sinf(2.0f * (float)M_PI * (50.0f * ph + 100.0f * 0.03f * (1.0f - expf(-ph / 0.03f))))
                           * expf(-ph / 0.12f) + 0.3f * noise() * expf(-ph / 0.004f);
        const float bph = fmodf((float)*t + period / 2, period);
        const float bass = 0.4f * sinf(2.0f * (float)M_PI * 55.0f * bph) * (bph < period * 0.4f ? 1.0f : 0.0f);
        const float hat = 0.15f * noise() * expf(-fmodf((float)*t + period / 4, period / 2) / 0.015f);
        buf[i] = amp * (0.8f * kick + bass + hat + 0.03f * noise());
    }
}

// Fraction of kicks detected over the last `measure` seconds (beats per kick).
static float beat_recall(float bpm, float amp_db, float hpf_hz, float *false_per_min)
{
    static mic_t m;
    mic_init(&m);
    m.hpf_hz = hpf_hz;
    float buf[AUDIO_FRAME];
    double t = 0;
    const float amp = powf(10.0f, (amp_db + 13.0f) / 20.0f);  // synth_track RMS is about -13 dB at amp 1
    const float secs = 30.0f, measure = 20.0f;
    const int frames = FRAMES(secs), from = frames - FRAMES(measure);
    uint32_t b0 = 0;
    for (int f = 0; f < frames; f++) {
        if (amp_db > -200.0f) synth_track(buf, AUDIO_FRAME, bpm, amp, &t);
        else for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = 0.0f;
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] += ROOM_AMP * noise();
        mic_feed(&m, buf);
        if (f == from) b0 = m.a.out.beat_count;
    }
    const float beats = (float)(m.a.out.beat_count - b0);
    if (false_per_min) *false_per_min = beats / measure * 60.0f;
    return beats / (measure * bpm / 60.0f);
}

// Beats through a mic that rolls off below 120 Hz, from loud down to the
// quiet end of the music measured on the board (-83 dB), plus false beats.
static void test_beat_sensitivity(void)
{
    const float levels[] = { -60, -70, -76, -80, -83 };
    // EDM, trance, dubstep, drum and bass, and faster.
    const float bpms[] = { 128, 138, 140, 174, 190 };
    for (int i = 0; i < 5; i++) {
        float lo = 9.0f, hi = 0.0f;
        for (int j = 0; j < 5; j++) {
            float r = beat_recall(bpms[j], levels[i], 120.0f, NULL);
            lo = fminf(lo, r);
            hi = fmaxf(hi, r);
        }
        CHECK(lo >= 0.85f && hi <= 2.05f, "kicks at %.0f dB (128-190 bpm): %.0f-%.0f%% detected", levels[i], lo * 100, hi * 100);
    }
    float fpm;
    beat_recall(120, -999.0f, 120.0f, &fpm);
    CHECK(fpm == 0.0f, "silent room: %.1f false beats/min", fpm);
    for (int kind = 0; kind < 3; kind++) {
        static mic_t m;
        mic_init(&m);
        m.hpf_hz = 120.0f;
        float buf[AUDIO_FRAME];
        double t = 0;
        uint32_t b0 = 0;
        for (int f = 0; f < FRAMES(40); f++) {
            for (int i = 0; i < AUDIO_FRAME; i++, t += 1.0 / AUDIO_SAMPLE_RATE) {
                float v;
                if (kind == 0) v = 0.02f * noise();                                   // crowd noise, -40 dB
                else if (kind == 1) v = 0.01f * (sinf(2 * M_PI * 110 * t) + sinf(2 * M_PI * 138.6 * t) +
                                               sinf(2 * M_PI * 164.8 * t));          // sustained chord
                else {                                                                // talking: irregular syllables
                    static double syl_start, syl_len = 0.2, gap_until;
                    if (t >= gap_until) {  // next syllable 0.12-0.35 s long, then a 0.03-0.4 s gap
                        syl_start = t;
                        syl_len = 0.12 + 0.23 * (noise() * 0.5 + 0.5);
                        gap_until = t + syl_len + 0.03 + 0.37 * (noise() * 0.5 + 0.5);
                    }
                    const double u = (t - syl_start) / syl_len;
                    const float env = u < 1.0 ? sinf((float)M_PI * (float)u) : 0.0f;
                    const float pitch = 120.0f + 40.0f * sinf(2 * M_PI * 0.7 * t);
                    v = 0.02f * env * (sinf(2 * M_PI * pitch * t) + 0.5f * sinf(4 * M_PI * pitch * t) + 0.3f * noise());
                }
                buf[i] = v + ROOM_AMP * noise();
            }
            mic_feed(&m, buf);
            if (f == FRAMES(20)) b0 = m.a.out.beat_count;
        }
        static const char *names[] = { "crowd noise", "sustained chord", "talking" };
        // A held chord can wobble just periodically enough for the odd pulse
        // (a stricter gate would cost real kicks); talking has real onsets.
        static const float limits[] = { 5.0f, 8.0f, 30.0f };
        const float per_min = (m.a.out.beat_count - b0) / 20.0f * 60.0f;
        CHECK(per_min <= limits[kind], "%s: %.0f false beats/min", names[kind], per_min);
    }
}

static void test_agc(void)
{
    static mic_t m;
    float buf[AUDIO_FRAME];
    double t = 0;

    // Festival-loud: clips at the starting gain, must back off and stop clipping.
    mic_init(&m);
    for (int f = 0; f < FRAMES(10); f++) { synth_music(buf, AUDIO_FRAME, 128, 1.0f, &t); mic_feed(&m, buf); }
    m.clipped = 0;
    for (int f = 0; f < FRAMES(10); f++) { synth_music(buf, AUDIO_FRAME, 128, 1.0f, &t); mic_feed(&m, buf); }
    CHECK(m.clipped == 0, "very loud music: %d clipped frames after settling (gain %.0f dB)", m.clipped, m.a.out.gain_db);

    // Quiet: gain climbs until peaks are in a healthy range.
    mic_init(&m);
    float env = -120.0f, ramp_s = -1.0f;
    for (int f = 0; f < FRAMES(45); f++) {
        synth_music(buf, AUDIO_FRAME, 128, 0.003f, &t);
        mic_feed(&m, buf);
        if (f > FRAMES(40)) env = fmaxf(env, m.peak_db);
        if (ramp_s < 0.0f && m.a.out.gain_db >= MIC_GAIN_MAX_DB - 6.0f) ramp_s = f * (float)AUDIO_FRAME / AUDIO_SAMPLE_RATE;
    }
    CHECK(ramp_s > 0.0f && ramp_s < 6.0f, "quiet music: gain ramped from %.0f to %.0f dB in %.1f s",
          MIC_GAIN_START_DB, MIC_GAIN_MAX_DB - 6.0f, ramp_s);
    CHECK(m.a.out.gain_db > MIC_GAIN_START_DB + 12.0f, "quiet music: gain raised to %.0f dB", m.a.out.gain_db);
    CHECK(env > AGC_TARGET_PEAK_DB - 9.0f && env < AGC_CLIP_DB, "quiet music: raw peaks now %.1f dBFS", env);
}

static void test_silence(void)
{
    static mic_t m;
    mic_init(&m);
    float buf[AUDIO_FRAME];
    for (int f = 0; f < FRAMES(20); f++) {
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = ROOM_AMP * noise();
        mic_feed(&m, buf);
    }
    CHECK(m.a.out.beat_count == 0, "silence: %u beats", (unsigned)m.a.out.beat_count);
    CHECK(m.a.out.loudness < 0.05f, "silence: loudness %.3f", m.a.out.loudness);
    CHECK(m.a.out.avg_db < QUIET_DB, "quiet room: average %.1f dB is below QUIET_DB", m.a.out.avg_db);
}

// Beats still come through over loud crowd noise.
static void test_crowd(void)
{
    static mic_t m;
    mic_init(&m);
    float buf[AUDIO_FRAME], music[AUDIO_FRAME];
    double t = 0;
    for (int f = 0; f < FRAMES(10); f++) {
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = 0.05f * noise();
        mic_feed(&m, buf);
    }
    uint32_t beats0 = m.a.out.beat_count;
    for (int f = 0; f < FRAMES(15); f++) {
        synth_music(music, AUDIO_FRAME, 120, 0.4f, &t);
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = music[i] + 0.05f * noise();
        mic_feed(&m, buf);
    }
    float bpm = (m.a.out.beat_count - beats0) / 15.0f * 60.0f;
    CHECK(bpm > 110.0f && bpm < 245.0f, "music over crowd: %.0f beats/min", bpm);
}

typedef struct {
    mic_t m;
    eye_t e;
    double t;
} scene_t;

// Run `secs` of faint-or-louder music (music_amp, 0 for none) over room noise,
// plus an optional burst at burst_amp for the first burst_s seconds.
static void scene_run(scene_t *sc, float secs, float music_amp, float burst_amp, float burst_s,
                      int *not_awake, float *wake_at)
{
    static const motion_features_t still = { 0 };
    float buf[AUDIO_FRAME], music[AUDIO_FRAME];
    const float dt = (float)AUDIO_FRAME / AUDIO_SAMPLE_RATE;
    for (int f = 0; f < FRAMES(secs); f++) {
        synth_music(music, AUDIO_FRAME, 110, music_amp, &sc->t);
        const bool burst = f * dt < burst_s;
        for (int i = 0; i < AUDIO_FRAME; i++)
            buf[i] = music[i] + ROOM_AMP * noise() + (burst ? burst_amp * noise() : 0.0f);
        mic_feed(&sc->m, buf);
        eye_update(&sc->e, &sc->m.a.out, &still, dt);
        if (not_awake) *not_awake += sc->e.state != EYE_AWAKE;
        if (wake_at && *wake_at < 0.0f && sc->e.state != EYE_ASLEEP && sc->e.state != EYE_DROWSY) *wake_at = f * dt;
    }
}

static void test_sleep_end_to_end(void)
{
    static scene_t sc;
    mic_init(&sc.m);
    eye_init(&sc.e, 7);

    int not_awake = 0;
    scene_run(&sc, 90, FAINT_MUSIC, 0, 0, &not_awake, NULL);
    CHECK(not_awake == 0, "90 s of faint background music: eye stayed awake (%d frames not awake, avg %.1f dB)",
          not_awake, sc.m.a.out.avg_db);
    not_awake = 0;
    scene_run(&sc, 60, FAINT_MUSIC * 0.8f, 0, 0, &not_awake, NULL);
    CHECK(not_awake == 0, "60 s of even fainter music: eye stayed awake (%d frames not awake, avg %.1f dB)",
          not_awake, sc.m.a.out.avg_db);

    scene_run(&sc, 45, 0, 0, 0, NULL, NULL);
    CHECK(sc.e.state == EYE_ASLEEP, "music stops, quiet room for 45 s: state %s (avg %.1f dB)",
          eye_state_name(sc.e.state), sc.m.a.out.avg_db);

    float wake_at = -1.0f;
    scene_run(&sc, 2, 0, 0.0002f, 0.2f, NULL, &wake_at);  // a soft sound (~-79 dB, 0.2 s)
    CHECK(wake_at >= 0.0f && wake_at < 0.5f, "short soft sound while asleep: woke after %.2f s", wake_at);

    scene_run(&sc, 45, 0, 0, 0, NULL, NULL);
    wake_at = -1.0f;
    scene_run(&sc, 5, FAINT_MUSIC, 0, 0, NULL, &wake_at);
    CHECK(wake_at >= 0.0f && wake_at < 3.0f, "faint music starts while asleep: woke after %.2f s", wake_at);

    // A loud crowd between sets is not a quiet room: stay awake.
    scene_run(&sc, 5, 0.3f, 0, 0, NULL, NULL);
    not_awake = 0;
    for (int f = 0; f < 60; f++) scene_run(&sc, 1, 0, 0.05f, 1.0f, &not_awake, NULL);
    CHECK(not_awake == 0, "60 s of loud crowd noise, no music: eye stayed awake (%d frames not awake)", not_awake);
}

static void test_warmth(void)
{
    static mic_t m;
    mic_init(&m);
    float buf[AUDIO_FRAME];
    double t = 0;
    // Balanced mix for 8 s, then bass only, then treble only.
    for (int f = 0; f < 250; f++) {
        for (int i = 0; i < AUDIO_FRAME; i++, t += 1.0 / AUDIO_SAMPLE_RATE)
            buf[i] = 0.2f * sinf(2 * M_PI * 80 * t) + 0.05f * sinf(2 * M_PI * 4000 * t);
        mic_feed(&m, buf);
    }
    for (int f = 0; f < 40; f++) {
        for (int i = 0; i < AUDIO_FRAME; i++, t += 1.0 / AUDIO_SAMPLE_RATE)
            buf[i] = 0.4f * sinf(2 * M_PI * 80 * t) + 0.01f * sinf(2 * M_PI * 4000 * t);
        mic_feed(&m, buf);
    }
    float warm = m.a.out.warmth;
    for (int f = 0; f < 80; f++) {
        for (int i = 0; i < AUDIO_FRAME; i++, t += 1.0 / AUDIO_SAMPLE_RATE)
            buf[i] = 0.04f * sinf(2 * M_PI * 80 * t) + 0.2f * sinf(2 * M_PI * 4000 * t);
        mic_feed(&m, buf);
    }
    float cool = m.a.out.warmth;
    CHECK(warm > 0.7f, "bass-heavy section: warmth %.2f", warm);
    CHECK(cool < 0.3f, "treble-heavy section: warmth %.2f", cool);
}

static void feed_motion(motion_analysis_t *m, float seconds, float bob_hz, float bob_g, float noise_g)
{
    static double t;
    for (int i = 0; i < (int)(seconds * IMU_RATE_HZ); i++, t += 1.0 / IMU_RATE_HZ) {
        float bob = bob_g * sinf(2 * M_PI * bob_hz * t);
        float acc[3] = { noise_g * noise(), noise_g * noise(), 1.0f + bob + noise_g * noise() };
        float gyro[3] = { 2 * noise(), 2 * noise(), 2 * noise() };
        motion_analysis_update(m, acc, gyro);
    }
}

static void test_dance(void)
{
    static motion_analysis_t m;
    motion_analysis_init(&m, IMU_RATE_HZ);
    feed_motion(&m, 8.0f, 2.0f, 0.3f, 0.02f);
    CHECK(m.out.dance_score > DANCE_HYPE_SCORE, "2 Hz bobbing: dance score %.2f", m.out.dance_score);
    CHECK(fabsf(m.out.dance_period_s - 0.5f) < 0.05f, "2 Hz bobbing: period %.2f s", m.out.dance_period_s);

    motion_analysis_init(&m, IMU_RATE_HZ);
    feed_motion(&m, 8.0f, 0.0f, 0.0f, 0.02f);
    CHECK(m.out.dance_score < 0.2f, "standing still: dance score %.2f", m.out.dance_score);
}

// With the board upright, the accelerometer reads +1 g toward the top of the
// screen. Map screen-up back into IMU axes using the configured mounting.
static void screen_to_imu(float sx, float sy, float out[3])
{
    float rx = sx * IMU_X_SIGN, ry = sy * IMU_Y_SIGN;
#if EYE_MOUNT_ROTATION == 90
    out[0] = ry; out[1] = -rx;
#elif EYE_MOUNT_ROTATION == 180
    out[0] = -rx; out[1] = -ry;
#elif EYE_MOUNT_ROTATION == 270
    out[0] = -ry; out[1] = rx;
#else
    out[0] = rx; out[1] = ry;
#endif
    out[2] = 0.0f;
}

static void test_screen_directions(void)
{
    static motion_analysis_t m;
    float gyro[3] = { 0 }, still[3], moving[3];
    screen_to_imu(0.0f, -1.0f, still);  // upright: reading points to screen top
    motion_analysis_init(&m, IMU_RATE_HZ);
    for (int i = 0; i < 800; i++) motion_analysis_update(&m, still, gyro);
    CHECK(m.out.look_y > 0.1f, "upright at rest: eye looks down (y=%+.2f)", m.out.look_y);
    CHECK(fabsf(m.out.look_x) < 0.05f, "upright at rest: no sideways look (x=%+.2f)", m.out.look_x);
    float rest_y = m.out.look_y;

    // Accelerate toward the bottom of the screen: reading along screen-up drops.
    screen_to_imu(0.0f, -1.0f + 0.5f, moving);
    for (int i = 0; i < 20; i++) motion_analysis_update(&m, moving, gyro);
    CHECK(m.out.look_y < rest_y - 0.2f, "moved down: pupil lags upward (y %+.2f -> %+.2f)", rest_y, m.out.look_y);
    CHECK(fabsf(m.out.look_x) < 0.05f, "moved down: no sideways drift (x=%+.2f)", m.out.look_x);
}

static void test_look_inertia(void)
{
    static motion_analysis_t m;
    float gyro[3] = { 0 }, still[3] = { 0, 0, 1 }, push[3];
    screen_to_imu(0.5f, 0.0f, push);  // accelerate toward the right of the screen
    push[2] = 1.0f;
    motion_analysis_init(&m, IMU_RATE_HZ);
    for (int i = 0; i < 400; i++) motion_analysis_update(&m, still, gyro);
    for (int i = 0; i < 20; i++) motion_analysis_update(&m, push, gyro);
    CHECK(m.out.look_x < -0.2f, "push toward screen right: pupil lags left (x=%+.2f)", m.out.look_x);
    CHECK(fabsf(m.out.look_y) < 0.05f, "push toward screen right: no vertical drift (y=%+.2f)", m.out.look_y);
    for (int i = 0; i < 800; i++) motion_analysis_update(&m, still, gyro);
    CHECK(fabsf(m.out.look_x) < 0.05f, "after settling: pupil back at x=%+.2f", m.out.look_x);
}

static void test_hype(void)
{
    static eye_t e;
    eye_init(&e, 1);
    // Moderate loudness, so the target glow is well below full and a runaway would show.
    audio_features_t music = { .level_db = -30.0f, .avg_db = -30.0f, .loudness = 0.2f, .warmth = 0.5f, .beat_period_s = 0.5f };
    motion_features_t dancing = { .dance_score = 0.9f, .dance_period_s = 0.5f, .energy_g = 0.3f };
    const float dt = 1.0f / 30;
    float phase_moved = 0.0f, prev = 0.0f;
    for (int i = 0; i < 90; i++) {  // 3 s
        eye_update(&e, &music, &dancing, dt);
        float d = e.p.ring_phase - prev;
        phase_moved += d < 0.0f ? d + 1.0f : d;
        prev = e.p.ring_phase;
    }
    CHECK(e.p.hype > 0.9f, "dancing to music: hype %.2f", e.p.hype);
    CHECK(e.p.intensity < 0.9f, "dancing to moderate music: glow %.2f settles, no runaway", e.p.intensity);
    CHECK(phase_moved > 3.5f && phase_moved < 6.5f, "dancing at 120 bpm: rings flowed %.1f rings in 3 s", phase_moved);
    motion_features_t still = { 0 };
    for (int i = 0; i < 90; i++) eye_update(&e, &music, &still, dt);
    CHECK(e.p.hype < 0.1f, "stopped dancing: hype falls to %.2f", e.p.hype);
}

static void test_sleep_wake(void)
{
    static eye_t e;
    eye_init(&e, 1);
    audio_features_t quiet = { .level_db = -90.0f, .avg_db = -90.0f, .warmth = 0.5f };
    motion_features_t still = { 0 };
    const float dt = 1.0f / 30;
    for (int i = 0; i < (int)(30 / dt); i++) eye_update(&e, &quiet, &still, dt);
    CHECK(e.state == EYE_ASLEEP, "30 s of silence: state %s", eye_state_name(e.state));
    audio_features_t loud = { .level_db = -30.0f, .avg_db = -30.0f, .loudness = 0.8f, .warmth = 0.5f, .beat_count = 1 };
    for (int i = 0; i < (int)(1.0f / dt); i++) eye_update(&e, &loud, &still, dt);
    CHECK(e.state == EYE_AWAKE, "music starts: state %s", eye_state_name(e.state));
}

// Optional: run a raw recording from the board (mono int16 at 16 kHz, captured
// at a fixed mic gain) through the analysis. Set NIBBLES_PCM=path[:gain_db].
static void analyse_recording(const char *spec)
{
    char path[512];
    float gain = 36.0f;
    snprintf(path, sizeof(path), "%s", spec);
    char *colon = strrchr(path, ':');
    if (colon) { gain = strtof(colon + 1, NULL); *colon = 0; }
    FILE *fp = fopen(path, "rb");
    if (!fp) { printf("cannot open %s\n", path); return; }
    static audio_analysis_t a;
    audio_analysis_init(&a, AUDIO_SAMPLE_RATE, gain);
    int16_t raw[AUDIO_FRAME];
    float buf[AUDIO_FRAME], beats[512];
    int nb = 0, f = 0;
    uint32_t last = 0;
    while (fread(raw, sizeof(int16_t), AUDIO_FRAME, fp) == AUDIO_FRAME) {
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = raw[i] / 32768.0f;
        audio_analysis_process(&a, buf);
        a.out.gain_db = a.applied_gain_db = gain;  // the recording's gain is fixed
        if (a.out.beat_count != last && nb < 512) beats[nb++] = f * (float)AUDIO_FRAME / AUDIO_SAMPLE_RATE;
        last = a.out.beat_count;
        f++;
    }
    fclose(fp);
    const float secs = f * (float)AUDIO_FRAME / AUDIO_SAMPLE_RATE;
    float ioi[511], sorted[511];
    int n = 0;
    for (int i = 1; i < nb; i++) ioi[n++] = beats[i] - beats[i - 1];
    memcpy(sorted, ioi, n * sizeof(float));
    for (int i = 1; i < n; i++) for (int j = i; j > 0 && sorted[j - 1] > sorted[j]; j--) { float t = sorted[j]; sorted[j] = sorted[j - 1]; sorted[j - 1] = t; }
    const float med = n ? sorted[n / 2] : 0.0f;
    int ok = 0;
    for (int i = 0; i < n; i++) {
        const float r = ioi[i] / med;
        if (fabsf(r - 0.5f) < 0.06f || fabsf(r - 1.0f) < 0.12f || fabsf(r - 2.0f) < 0.24f) ok++;
    }
    printf("recording %.1f s: %d beats, median tempo %.1f bpm, %.0f%% of gaps on-tempo, final tempo %.1f bpm\n",
           secs, nb, med > 0 ? 60.0f / med : 0.0f, n ? 100.0f * ok / n : 0.0f,
           a.out.beat_period_s > 0 ? 60.0f / a.out.beat_period_s : 0.0f);
}

int main(void)
{
    const char *pcm = getenv("NIBBLES_PCM");
    if (pcm) { analyse_recording(pcm); return 0; }
    srand(42);
    test_beats(128.0f, 0.3f, 20, 15, "EDM");
    test_beats(138.0f, 0.3f, 20, 15, "trance");
    test_beats(70.0f, 0.3f, 20, 15, "dubstep half-time");
    test_beats(174.0f, 0.3f, 20, 15, "drum and bass");
    test_beats(190.0f, 0.3f, 20, 15, "fast");
    test_beats(138.0f, 0.003f, 45, 15, "quiet trance");
    test_beat_sensitivity();
    test_agc();
    test_silence();
    test_crowd();
    test_sleep_end_to_end();
    test_warmth();
    test_dance();
    test_look_inertia();
    test_screen_directions();
    test_hype();
    test_sleep_wake();
    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
