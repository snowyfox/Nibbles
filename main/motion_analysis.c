#include "motion_analysis.h"

#include <math.h>
#include <string.h>

#define EVAL_EVERY   12      // dance evaluation every 12 decimated samples (~0.24 s)

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// Rotate an IMU-frame x/y pair into screen coordinates.
static void to_screen(float x, float y, float *sx, float *sy)
{
#if EYE_MOUNT_ROTATION == 90
    float rx = -y, ry = x;
#elif EYE_MOUNT_ROTATION == 180
    float rx = -x, ry = -y;
#elif EYE_MOUNT_ROTATION == 270
    float rx = y, ry = -x;
#else
    float rx = x, ry = y;
#endif
    *sx = rx * IMU_X_SIGN;
    *sy = ry * IMU_Y_SIGN;
}

void motion_analysis_init(motion_analysis_t *m, float rate_hz)
{
    memset(m, 0, sizeof(*m));
    m->dt = 1.0f / rate_hz;
    m->decim = (int)(rate_hz / DANCE_RATE_HZ + 0.5f);
    if (m->decim < 1) m->decim = 1;
}

// Strongest normalised autocorrelation peak within the dance period range.
static float best_period(const float *buf, int n, int start, float *period_s)
{
    float x[DANCE_SAMPLES];
    float mean = 0.0f;
    for (int i = 0; i < n; i++) mean += buf[(start + i) % n];
    mean /= n;
    float r0 = 0.0f;
    for (int i = 0; i < n; i++) {
        x[i] = buf[(start + i) % n] - mean;
        r0 += x[i] * x[i];
    }
    if (r0 < 1e-9f) return 0.0f;

    const int lo = (int)(DANCE_MIN_PERIOD_S * DANCE_RATE_HZ);
    const int hi = (int)(DANCE_MAX_PERIOD_S * DANCE_RATE_HZ);
    float r[DANCE_SAMPLES / 2 + 2];
    for (int lag = lo - 1; lag <= hi + 1 && lag < n / 2; lag++) {
        float s = 0.0f;
        for (int i = 0; i + lag < n; i++) s += x[i] * x[i + lag];
        // Scale for the shorter overlap so long lags aren't penalised.
        r[lag] = s / r0 * (float)n / (n - lag);
    }
    // A periodic signal also peaks at multiples of its period, so take the
    // shortest lag whose peak is nearly as strong as the strongest one.
    float best = 0.0f;
    for (int lag = lo; lag <= hi && lag + 1 < n / 2; lag++) {
        if (r[lag] > best && r[lag] >= r[lag - 1] && r[lag] >= r[lag + 1]) best = r[lag];
    }
    for (int lag = lo; lag <= hi && lag + 1 < n / 2; lag++) {
        if (r[lag] >= 0.85f * best && r[lag] >= r[lag - 1] && r[lag] >= r[lag + 1]) {
            *period_s = (float)lag / DANCE_RATE_HZ;
            break;
        }
    }
    return best;
}

void motion_analysis_update(motion_analysis_t *m, const float acc[3], const float gyro[3])
{
    motion_features_t *o = &m->out;
    const float dt = m->dt;

    // Gravity estimate and linear acceleration. Seed it from the first plausible
    // reading; the IMU can return zeros right after power-up.
    if (!m->have_grav) {
        float n2 = acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2];
        if (n2 < 0.25f || n2 > 2.25f) return;
        memcpy(m->grav, acc, sizeof(m->grav));
        m->have_grav = 1;
    }
    float a = dt / (GRAVITY_TAU_S + dt);
    float lin[3];
    for (int i = 0; i < 3; i++) {
        m->grav[i] += a * (acc[i] - m->grav[i]);
        lin[i] = acc[i] - m->grav[i];
    }
    float gmag = sqrtf(m->grav[0] * m->grav[0] + m->grav[1] * m->grav[1] + m->grav[2] * m->grav[2]);
    float linmag = sqrtf(lin[0] * lin[0] + lin[1] * lin[1] + lin[2] * lin[2]);
    o->jolt_g = fmaxf(o->jolt_g * expf(-dt / 0.15f), linmag);

    // Pupil: mass on a spring, pushed against the direction of motion.
    float lx, ly, gx, gy, wx, wy;
    to_screen(lin[0], lin[1], &lx, &ly);
    to_screen(m->grav[0], m->grav[1], &gx, &gy);
    to_screen(gyro[0], gyro[1], &wx, &wy);
    float inv_g = gmag > 0.1f ? 1.0f / gmag : 0.0f;
    // At rest the accelerometer reads +1 g pointing up, so "down" is -grav.
    float target[2] = {
        -lx * LOOK_ACCEL_GAIN + wy * LOOK_GYRO_GAIN - gx * inv_g * LOOK_DOWN_BIAS,
        -ly * LOOK_ACCEL_GAIN - wx * LOOK_GYRO_GAIN - gy * inv_g * LOOK_DOWN_BIAS,
    };
    const float w = 2.0f * (float)M_PI * LOOK_SPRING_HZ;
    for (int i = 0; i < 2; i++) {
        float acc_p = w * w * (target[i] - m->pos[i]) - 2.0f * LOOK_DAMPING * w * m->vel[i];
        m->vel[i] += acc_p * dt;
        m->pos[i] += m->vel[i] * dt;
    }
    float r = sqrtf(m->pos[0] * m->pos[0] + m->pos[1] * m->pos[1]);
    if (r > 1.0f) {
        m->pos[0] /= r;
        m->pos[1] /= r;
        m->vel[0] *= 0.5f;
        m->vel[1] *= 0.5f;
    }
    o->look_x = m->pos[0];
    o->look_y = m->pos[1];

    // Dance: decimate vertical (along gravity) and screen-horizontal linear accel.
    m->acc_v += (lin[0] * m->grav[0] + lin[1] * m->grav[1] + lin[2] * m->grav[2]) * inv_g;
    m->acc_h += lx;
    if (++m->decim_n < m->decim) return;
    m->buf_v[m->buf_pos] = m->acc_v / m->decim_n;
    m->buf_h[m->buf_pos] = m->acc_h / m->decim_n;
    m->acc_v = m->acc_h = 0.0f;
    m->decim_n = 0;
    m->buf_pos = (m->buf_pos + 1) % DANCE_SAMPLES;
    if (m->buf_filled < DANCE_SAMPLES) m->buf_filled++;
    if (m->buf_filled < DANCE_SAMPLES || ++m->since_eval < EVAL_EVERY) return;
    m->since_eval = 0;

    float e = 0.0f;
    for (int i = 0; i < DANCE_SAMPLES; i++) e += m->buf_v[i] * m->buf_v[i] + m->buf_h[i] * m->buf_h[i];
    o->energy_g = sqrtf(e / DANCE_SAMPLES);

    float pv = 0.0f, ph = 0.0f;
    float rv = best_period(m->buf_v, DANCE_SAMPLES, m->buf_pos, &pv);
    float rh = best_period(m->buf_h, DANCE_SAMPLES, m->buf_pos, &ph);
    float rbest = rv >= rh ? rv : rh;
    float period = rv >= rh ? pv : ph;

    float raw = clampf((rbest - 0.25f) / 0.45f, 0.0f, 1.0f) *
                clampf(o->energy_g / DANCE_MIN_ENERGY_G, 0.0f, 1.0f);
    o->dance_score += 0.4f * (raw - o->dance_score);
    o->dance_period_s = raw > 0.2f ? period : 0.0f;
}
