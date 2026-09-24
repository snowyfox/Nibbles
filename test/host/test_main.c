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

static void test_beats(float bpm)
{
    static audio_analysis_t a;
    audio_analysis_init(&a, AUDIO_SAMPLE_RATE);
    float buf[AUDIO_FRAME];
    double t = 0;
    const int frames = (int)(20.0f * AUDIO_SAMPLE_RATE / AUDIO_FRAME);  // 20 s
    const int frames_5s = (int)(5.0f * AUDIO_SAMPLE_RATE / AUDIO_FRAME);
    uint32_t beats_at_5s = 0;
    float loud_sum = 0.0f;
    for (int f = 0; f < frames; f++) {
        synth_music(buf, AUDIO_FRAME, bpm, 0.3f, &t);
        audio_analysis_process(&a, buf);
        if (f == frames_5s) beats_at_5s = a.out.beat_count;
        if (f > frames_5s) loud_sum += a.out.loudness;
    }
    float loud_avg = loud_sum / (frames - frames_5s - 1);
    float detected_bpm = (a.out.beat_count - beats_at_5s) / 15.0f * 60.0f;
    float period_bpm = a.out.beat_period_s > 0 ? 60.0f / a.out.beat_period_s : 0;
    CHECK(fabsf(detected_bpm - bpm) < bpm * 0.08f, "%.0f bpm track: %.1f beats/min counted", bpm, detected_bpm);
    CHECK(fabsf(period_bpm - bpm) < bpm * 0.08f, "%.0f bpm track: tempo from beat period %.1f bpm", bpm, period_bpm);
    CHECK(loud_avg > 0.3f, "%.0f bpm track: average loudness %.2f while music plays", bpm, loud_avg);
}

static void test_silence(void)
{
    static audio_analysis_t a;
    audio_analysis_init(&a, AUDIO_SAMPLE_RATE);
    float buf[AUDIO_FRAME];
    for (int f = 0; f < 300; f++) {
        for (int i = 0; i < AUDIO_FRAME; i++) buf[i] = 0.0002f * noise();
        audio_analysis_process(&a, buf);
    }
    CHECK(a.out.beat_count == 0, "silence: %u beats", (unsigned)a.out.beat_count);
    CHECK(a.out.loudness < 0.05f, "silence: loudness %.3f", a.out.loudness);
    CHECK(a.out.level_db < SILENCE_DB, "silence: level %.1f dBFS below SILENCE_DB", a.out.level_db);
}

static void test_warmth(void)
{
    static audio_analysis_t a;
    audio_analysis_init(&a, AUDIO_SAMPLE_RATE);
    float buf[AUDIO_FRAME];
    double t = 0;
    // Balanced mix for 8 s, then bass only, then treble only.
    for (int f = 0; f < 250; f++) {
        for (int i = 0; i < AUDIO_FRAME; i++, t += 1.0 / AUDIO_SAMPLE_RATE)
            buf[i] = 0.2f * sinf(2 * M_PI * 80 * t) + 0.05f * sinf(2 * M_PI * 4000 * t);
        audio_analysis_process(&a, buf);
    }
    for (int f = 0; f < 40; f++) {
        for (int i = 0; i < AUDIO_FRAME; i++, t += 1.0 / AUDIO_SAMPLE_RATE)
            buf[i] = 0.4f * sinf(2 * M_PI * 80 * t) + 0.01f * sinf(2 * M_PI * 4000 * t);
        audio_analysis_process(&a, buf);
    }
    float warm = a.out.warmth;
    for (int f = 0; f < 80; f++) {
        for (int i = 0; i < AUDIO_FRAME; i++, t += 1.0 / AUDIO_SAMPLE_RATE)
            buf[i] = 0.04f * sinf(2 * M_PI * 80 * t) + 0.2f * sinf(2 * M_PI * 4000 * t);
        audio_analysis_process(&a, buf);
    }
    float cool = a.out.warmth;
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
    CHECK(m.out.dance_score > DANCE_HAPPY_SCORE, "2 Hz bobbing: dance score %.2f", m.out.dance_score);
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

static void test_sleep_wake(void)
{
    static eye_t e;
    eye_init(&e, 1);
    audio_features_t quiet = { .level_db = -80.0f, .warmth = 0.5f };
    motion_features_t still = { 0 };
    const float dt = 1.0f / 30;
    for (int i = 0; i < (int)(30 / dt); i++) eye_update(&e, &quiet, &still, dt);
    CHECK(e.state == EYE_ASLEEP, "30 s of silence: state %s", eye_state_name(e.state));
    audio_features_t loud = { .level_db = -30.0f, .loudness = 0.8f, .warmth = 0.5f, .beat_count = 1 };
    for (int i = 0; i < (int)(1.0f / dt); i++) eye_update(&e, &loud, &still, dt);
    CHECK(e.state == EYE_AWAKE, "music starts: state %s", eye_state_name(e.state));
}

int main(void)
{
    srand(42);
    test_beats(120.0f);
    test_beats(128.0f);
    test_beats(90.0f);
    test_silence();
    test_warmth();
    test_dance();
    test_look_inertia();
    test_screen_directions();
    test_sleep_wake();
    printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
    return failures ? 1 : 0;
}
