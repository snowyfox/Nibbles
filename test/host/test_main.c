// Host tests for the pure-C analysis modules. Run with `make` in this folder.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

// Simulated microphone: signals are "acoustic" levels referred to 0 dB mic
// gain. The hardware gain lags the analysis' request by two frames (DMA
// buffering) and the ADC clips at full scale.
typedef struct {
    audio_analysis_t a;
    float hw_gain_db, queue[2];
    float peak_db;  // raw peak of the last frame
    int clipped;    // frames that hit full scale
} mic_t;

static void mic_init(mic_t *m)
{
    audio_analysis_init(&m->a, AUDIO_SAMPLE_RATE, MIC_GAIN_START_DB);
    m->hw_gain_db = m->queue[0] = m->queue[1] = MIC_GAIN_START_DB;
    m->clipped = 0;
}

static void mic_feed(mic_t *m, const float *acoustic)
{
    float raw[AUDIO_FRAME], peak = 0.0f;
    const float g = powf(10.0f, m->hw_gain_db / 20.0f);
    bool clip = false;
    for (int i = 0; i < AUDIO_FRAME; i++) {
        float v = acoustic[i] * g;
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
    CHECK(fabsf(detected_bpm - bpm) < bpm * 0.08f, "%s %.0f bpm: %.1f beats/min counted (gain %.0f dB)",
          label, bpm, detected_bpm, m.a.out.gain_db);
    CHECK(fabsf(period_bpm - bpm) < bpm * 0.08f, "%s %.0f bpm: tempo from beat period %.1f bpm", label, bpm, period_bpm);
    CHECK(loud_avg > 0.3f, "%s %.0f bpm: average loudness %.2f", label, bpm, loud_avg);
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
    float env = -120.0f;
    for (int f = 0; f < FRAMES(45); f++) {
        synth_music(buf, AUDIO_FRAME, 128, 0.003f, &t);
        mic_feed(&m, buf);
        if (f > FRAMES(40)) env = fmaxf(env, m.peak_db);
    }
    CHECK(m.a.out.gain_db > MIC_GAIN_START_DB + 12.0f, "quiet music: gain raised to %.0f dB", m.a.out.gain_db);
    CHECK(env > AGC_TARGET_PEAK_DB - 9.0f && env < AGC_CLIP_DB, "quiet music: raw peaks now %.1f dBFS", env);
}

static void test_silence(void)
{
    static mic_t m;
    mic_init(&m);
    float buf[AUDIO_FRAME];
    int sound = 0;
    for (int f = 0; f < FRAMES(40); f++) {  // the floor needs ~15 s to learn the room
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = 0.0002f * noise();
        mic_feed(&m, buf);
        if (f > FRAMES(30)) sound += m.a.out.sound;
    }
    CHECK(m.a.out.beat_count == 0, "silence: %u beats", (unsigned)m.a.out.beat_count);
    CHECK(m.a.out.loudness < 0.05f, "silence: loudness %.3f", m.a.out.loudness);
    CHECK(sound == 0, "silence: %d frames counted as sound", sound);
}

// Steady crowd noise with no music should count as quiet; music on top should not.
static void test_crowd(void)
{
    static mic_t m;
    mic_init(&m);
    float buf[AUDIO_FRAME];
    double t = 0;
    int sound = 0;
    for (int f = 0; f < FRAMES(40); f++) {
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = 0.05f * noise();
        mic_feed(&m, buf);
        if (f > FRAMES(20)) sound += m.a.out.sound;
    }
    CHECK(sound < FRAMES(20) / 50, "crowd noise only: %d of %d frames counted as sound", sound, FRAMES(20));

    sound = 0;
    uint32_t beats0 = m.a.out.beat_count;
    float music[AUDIO_FRAME];
    for (int f = 0; f < FRAMES(15); f++) {
        synth_music(music, AUDIO_FRAME, 120, 0.4f, &t);
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = music[i] + 0.05f * noise();
        mic_feed(&m, buf);
        sound += m.a.out.sound;
    }
    float bpm = (m.a.out.beat_count - beats0) / 15.0f * 60.0f;
    CHECK(sound > FRAMES(15) / 5, "music over crowd: %d of %d frames counted as sound", sound, FRAMES(15));
    CHECK(fabsf(bpm - 120.0f) < 15.0f, "music over crowd: %.0f beats/min", bpm);
}

static void test_sleep_end_to_end(void)
{
    static mic_t m;
    static eye_t e;
    mic_init(&m);
    eye_init(&e, 7);
    motion_features_t still = { 0 };
    float buf[AUDIO_FRAME], music[AUDIO_FRAME];
    double t = 0;
    const float dt = (float)AUDIO_FRAME / AUDIO_SAMPLE_RATE;
    int slept_in_music = 0;
    for (int f = 0; f < FRAMES(60); f++) {  // boot straight into loud music over crowd noise
        synth_music(music, AUDIO_FRAME, 126, 0.5f, &t);
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = music[i] + 0.05f * noise();
        mic_feed(&m, buf);
        eye_update(&e, &m.a.out, &still, dt);
        slept_in_music += e.state != EYE_AWAKE;
    }
    CHECK(slept_in_music == 0, "60 s of music from boot: eye stayed awake (%d frames not awake)", slept_in_music);
    for (int f = 0; f < FRAMES(45); f++) {  // set ends, crowd noise carries on
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = 0.05f * noise();
        mic_feed(&m, buf);
        eye_update(&e, &m.a.out, &still, dt);
    }
    CHECK(e.state == EYE_ASLEEP, "45 s of crowd noise after the set: state %s", eye_state_name(e.state));
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
    audio_features_t music = { .level_db = -30.0f, .sound = true, .loudness = 0.2f, .warmth = 0.5f, .beat_period_s = 0.5f };
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
    audio_features_t quiet = { .level_db = -80.0f, .warmth = 0.5f };
    motion_features_t still = { 0 };
    const float dt = 1.0f / 30;
    for (int i = 0; i < (int)(30 / dt); i++) eye_update(&e, &quiet, &still, dt);
    CHECK(e.state == EYE_ASLEEP, "30 s of silence: state %s", eye_state_name(e.state));
    audio_features_t loud = { .level_db = -30.0f, .sound = true, .loudness = 0.8f, .warmth = 0.5f, .beat_count = 1 };
    for (int i = 0; i < (int)(1.0f / dt); i++) eye_update(&e, &loud, &still, dt);
    CHECK(e.state == EYE_AWAKE, "music starts: state %s", eye_state_name(e.state));
}

int main(void)
{
    srand(42);
    test_beats(120.0f, 0.3f, 20, 15, "loud");
    test_beats(128.0f, 0.3f, 20, 15, "loud");
    test_beats(90.0f, 0.3f, 20, 15, "loud");
    test_beats(120.0f, 0.003f, 45, 15, "quiet");
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
