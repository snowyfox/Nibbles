#include "motion_analysis.h"

#include <math.h>
#include <string.h>

#define EVAL_EVERY   12      // dance evaluation every 12 decimated samples (~0.24 s)

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static float smoothstep(float e0, float e1, float x)
{
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

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

// Screen x/y back into IMU x/y (inverse of to_screen).
static void from_screen(float sx, float sy, float *x, float *y)
{
    const float rx = sx / IMU_X_SIGN, ry = sy / IMU_Y_SIGN;
#if EYE_MOUNT_ROTATION == 90
    *x = ry; *y = -rx;
#elif EYE_MOUNT_ROTATION == 180
    *x = -rx; *y = -ry;
#elif EYE_MOUNT_ROTATION == 270
    *x = -ry; *y = rx;
#else
    *x = rx; *y = ry;
#endif
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
    float gmag = sqrtf(m->grav[0] * m->grav[0] + m->grav[1] * m->grav[1] + m->grav[2] * m->grav[2]);
    float inv_g = gmag > 0.1f ? 1.0f / gmag : 0.0f;

    // Split rotation into twist about the vertical and tilt. At rest the
    // accelerometer reads +1 g pointing up, so grav/|grav| is "up" and "down" is -grav.
    const float up[3] = { m->grav[0] * inv_g, m->grav[1] * inv_g, m->grav[2] * inv_g };
    const float twist = gyro[0] * up[0] + gyro[1] * up[1] + gyro[2] * up[2];
    const float tilt[3] = { gyro[0] - twist * up[0], gyro[1] - twist * up[1], gyro[2] - twist * up[2] };
    o->twist_dps = twist;

    // "Sideways" for a twist is the true horizontal in the screen plane, which
    // follows the board's roll: perpendicular to up as seen on the screen,
    // pointing toward screen +x when the board is upright.
    float ux, uy;
    to_screen(up[0], up[1], &ux, &uy);
    const float ul = sqrtf(ux * ux + uy * uy);
    const float hx = ul > 0.2f ? -uy / ul : 1.0f, hy = ul > 0.2f ? ux / ul : 0.0f;

    // The eye sits TWIST_RADIUS_M from the twist axis, so speeding up or
    // slowing a twist swings it sideways (toward +h for a speeding-up
    // counter-clockwise twist). Remove that from the reading so it isn't
    // mistaken for sway.
    // Light smoothing only: a lag here leaves swing behind at fast twists.
    m->twist_accel += (dt / (0.005f + dt)) * ((twist - m->prev_twist) / dt - m->twist_accel);
    m->prev_twist = twist;
    const float swing_g = TWIST_SCREEN_SIGN * m->twist_accel * ((float)M_PI / 180.0f) * TWIST_RADIUS_M / 9.81f;
    float sx_imu, sy_imu;
    from_screen(hx, hy, &sx_imu, &sy_imu);
    const float acc_c[3] = { acc[0] - swing_g * sx_imu, acc[1] - swing_g * sy_imu, acc[2] };

    float a = dt / (GRAVITY_TAU_S + dt);
    float lin[3];
    for (int i = 0; i < 3; i++) {
        m->grav[i] += a * (acc_c[i] - m->grav[i]);
        lin[i] = acc_c[i] - m->grav[i];
    }
    float linmag = sqrtf(lin[0] * lin[0] + lin[1] * lin[1] + lin[2] * lin[2]);
    o->jolt_g = fmaxf(o->jolt_g * expf(-dt / 0.15f), linmag);

    // Activity: how hard the pole is moving, whatever the kind of motion.
    const float ka = dt / (ACTIVITY_AVG_S + dt);
    m->gyro_ms += ka * (gyro[0] * gyro[0] + gyro[1] * gyro[1] + gyro[2] * gyro[2] - m->gyro_ms);
    m->lin_ms += ka * (linmag * linmag - m->lin_ms);
    o->activity = fmaxf(smoothstep(ACTIVITY_GYRO_LO, ACTIVITY_GYRO_HI, sqrtf(m->gyro_ms)),
                        smoothstep(ACTIVITY_ACCEL_LO, ACTIVITY_ACCEL_HI, sqrtf(m->lin_ms)));

    // Pupil: mass on a spring, pushed against the direction of motion.
    float lx, ly, gx, gy, wx, wy;
    to_screen(lin[0], lin[1], &lx, &ly);
    to_screen(m->grav[0], m->grav[1], &gx, &gy);
    to_screen(tilt[0], tilt[1], &wx, &wy);
    // A twist carries the eye sideways (toward +h for a counter-clockwise twist
    // when the screen faces outward), so its lag pushes the pupil the other way.
    const float twist_push = -TWIST_SCREEN_SIGN * twist * TWIST_GAIN;
    const float side_push = -lx * LOOK_ACCEL_GAIN + wy * LOOK_GYRO_GAIN;

    // How much of the motion is twist, compared in look units. Twist is
    // noticed quickly when it starts and forgotten slowly, so the eye turns
    // into a twist straight away instead of first lagging behind it.
    const float k = dt / (TWIST_SMOOTH_S + dt);
    const float tw = fabsf(twist_push);
    m->twist_avg += (tw > m->twist_avg ? dt / (TWIST_ATTACK_S + dt) : k) * (tw - m->twist_avg);
    m->other_avg += k * ((fabsf(lx) + fabsf(ly)) * LOOK_ACCEL_GAIN + (fabsf(wx) + fabsf(wy)) * LOOK_GYRO_GAIN - m->other_avg);
    const float share = m->twist_avg / (m->twist_avg + m->other_avg + 1e-3f);
    o->twist_dominance = smoothstep(TWIST_DOMINANT_LO, TWIST_DOMINANT_HI, share);

    // Mostly twist: reverse left/right. Other sideways motion reverses through
    // the spring; the twist itself steers the pupil into the turn directly,
    // skipping the spring so it keeps up with fast twisting. Otherwise twist
    // lags through the spring like any other motion.
    const float dom = o->twist_dominance;
    const float ks = dt / (TWIST_SNAP_S + dt);
    m->twist_snap[0] += ks * (-twist_push * dom * hx - m->twist_snap[0]);
    m->twist_snap[1] += ks * (-twist_push * dom * hy - m->twist_snap[1]);
    const float twist_lag = twist_push * (1.0f - dom);
    float target[2] = {
        side_push * (1.0f - 2.0f * dom) + twist_lag * hx - gx * inv_g * LOOK_DOWN_BIAS,
        -ly * LOOK_ACCEL_GAIN - wx * LOOK_GYRO_GAIN + twist_lag * hy - gy * inv_g * LOOK_DOWN_BIAS,
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
    float lx_out = m->pos[0] + m->twist_snap[0], ly_out = m->pos[1] + m->twist_snap[1];
    r = sqrtf(lx_out * lx_out + ly_out * ly_out);
    if (r > 1.0f) {
        lx_out /= r;
        ly_out /= r;
    }
    o->look_x = lx_out;
    o->look_y = ly_out;

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
