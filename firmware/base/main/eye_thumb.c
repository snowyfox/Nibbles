#include "eye_thumb.h"

#include <math.h>
#include <string.h>

// The eye's own geometry, in its 466 px panel's pixels (firmware/eyes/main/config.h).
#define RADIUS       233
#define OUTLINE_R    218.0f
#define IRIS_R       150.0f
#define PUPIL_R      38.0f
#define ASLEEP_LID   0.0f
#define DROWSY_LID   0.45f
#define LID_CLEAR    32.0f
#define BINS         (RADIUS + 3)

static float acc[BINS][3];     // glow by distance from the centre, one bin per panel pixel
static float soft[BINS][3];    // ... averaged over a thumbnail pixel
static float arm_env[BINS][3]; // a spiral arm's colour by distance

static float smooth(float e0, float e1, float x)
{
    float t = (x - e0) / (e1 - e0);
    t = t < 0 ? 0 : t > 1 ? 1 : t;
    return t * t * (3 - 2 * t);
}

static float gauss(float x) { return exp(-0.5f * x * x); }

static void hsv(float h, float s, float out[3])
{
    h = fmodf(h, 360);
    if (h < 0) h += 360;
    const float c = s, x = c * (1 - fabsf(fmodf(h / 60, 2) - 1)), m = 1 - c;
    float r, g, b;
    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }
    out[0] = r + m; out[1] = g + m; out[2] = b + m;
}

static void colour_at(const preset_t *p, float pos, float out[3])
{
    if (p->rainbow) {
        hsv(pos * p->hue_spread, p->sat, out);
        return;
    }
    float t = pos * p->palette_step;
    t = (t - floorf(t)) * 4;
    const int i = (int)t % 4;
    const float f = t - floorf(t);
    const preset_rgb_t *a = &p->palette[i], *b = &p->palette[(i + 1) % 4];
    out[0] = a->r + (b->r - a->r) * f;
    out[1] = a->g + (b->g - a->g) * f;
    out[2] = a->b + (b->b - a->b) * f;
}

static void add_ring(float centre, float sigma, float amp, const float c[3])
{
    if (amp <= 0.001f) return;
    int lo = (int)(centre - 4 * sigma), hi = (int)(centre + 4 * sigma) + 1;
    if (lo < 0) lo = 0;
    if (hi > BINS) hi = BINS;
    for (int i = lo; i < hi; i++) {
        const float g = amp * gauss((i - centre) / sigma);
        for (int k = 0; k < 3; k++) acc[i][k] += g * c[k];
    }
}

static void add_band(float centre, float hw, float amp, const float c[3])
{
    int lo = (int)(centre - hw - 2), hi = (int)(centre + hw + 2) + 1;
    if (lo < 0) lo = 0;
    if (hi > BINS) hi = BINS;
    for (int i = lo; i < hi; i++) {
        const float g = amp * (1 - smooth(hw - 1, hw + 1, fabsf(i - centre)));
        for (int k = 0; k < 3; k++) acc[i][k] += g * c[k];
    }
}

static uint16_t to565(const float c[3])
{
    const int r = (int)(c[0] * 31 + 0.5f), g = (int)(c[1] * 63 + 0.5f), b = (int)(c[2] * 31 + 0.5f);
    return (uint16_t)((r > 31 ? 31 : r) << 11 | (g > 63 ? 63 : g) << 5 | (b > 31 ? 31 : b));
}

float eye_thumb_lid(int state)
{
    return state == 2 ? ASLEEP_LID : state == 1 ? DROWSY_LID : 1;   // EYE_ASLEEP, EYE_DROWSY
}

// The glow by distance, as render_core_prepare builds it for a still eye.
static void build_radial(const preset_t *p, float scale, float line[3])
{
    const float pr = PUPIL_R * p->pupil_scale;
    const int n = p->rings;
    float c[3];
    memset(acc, 0, sizeof(acc));
    colour_at(p, 0, c);
    for (int k = 0; k < 3; k++) c[k] += (1 - c[k]) * p->rim_white;
    add_ring(pr, 2, p->rim_amp, c);
    add_ring(pr, 7, 0.3f * p->rim_amp, c);
    if (p->fill > 0) {
        for (int i = (int)pr; i < IRIS_R + 10 && i < BINS; i++) {
            const float t = (i - pr) / (IRIS_R - pr);
            const float g = p->fill * smooth(0, 0.15f, t) * (1 - smooth(0.9f, 1.05f, t));
            colour_at(p, t * n, c);
            for (int k = 0; k < 3; k++) acc[i][k] += g * c[k];
        }
    }
    const float spacing = n > 0 ? (IRIS_R - pr) / n : 0;
    for (int i = 1; n > 0 && i <= n; i++) {
        const float at = pr + spacing * i;
        const float amp = 1 - 0.5f * i / n;
        colour_at(p, i, c);
        if (p->bands) add_band(at, p->ring_width, amp, c);
        else add_ring(at, p->ring_width, amp, c);
        add_ring(at, p->halo_sigma, p->halo_amp * amp, c);
    }
    colour_at(p, n, line);
    for (int i = 180; i < BINS; i++) {
        const float d = i - OUTLINE_R, g = gauss(d / 2.5f) + 0.3f * gauss(d / 10);
        for (int k = 0; k < 3; k++) acc[i][k] += g * line[k];
    }
    // A thumbnail pixel covers several panel pixels: average, so thin lines stay.
    const int half = (int)(scale / 2 + 0.5f);
    for (int i = 0; i < BINS; i++) {
        float sum[3] = { 0 };
        int count = 0;
        for (int j = i - half; j <= i + half; j++) {
            if (j < 0 || j >= BINS) continue;
            for (int k = 0; k < 3; k++) sum[k] += acc[j][k];
            count++;
        }
        for (int k = 0; k < 3; k++) soft[i][k] = sum[k] / count * (p->bands ? 1.0f : 1.25f);   // thin glows lose to the average
    }
    if (p->spiral_arms > 0) {
        for (int i = 0; i < BINS; i++) {
            const float env = smooth(pr, pr + 15, i) * (1 - smooth(IRIS_R + 20, OUTLINE_R - 10, i)) * (1 - 0.3f * i / OUTLINE_R);
            colour_at(p, i / 40.0f, c);
            for (int k = 0; k < 3; k++) arm_env[i][k] = env * c[k];
        }
    }
}

void eye_thumb_draw(uint16_t *px, int size, const preset_t *p, float lid_open, bool mirror)
{
    const float scale = (float)(2 * RADIUS) / size;
    float line[3];
    build_radial(p, scale, line);
    const float tau = 2 * (float)M_PI;
    for (int y = 0; y < size; y++) {
        const float dy = (y + 0.5f) * scale - RADIUS;
        for (int x = 0; x < size; x++) {
            const float dx = (x + 0.5f) * scale - RADIUS;
            const float d = sqrtf(dx * dx + dy * dy);
            uint16_t *out = &px[y * size + x];
            if (d >= RADIUS) { *out = 0; continue; }
            float col[3];
            const int bin = (int)d;
            memcpy(col, soft[bin], sizeof(col));
            if (p->spiral_arms > 0) {
                float turn = atan2f(dy, mirror ? -dx : dx) / tau * p->spiral_arms + d / p->spiral_twist;
                turn -= floorf(turn);
                const float g = 0.5f + 0.5f * cosf(tau * turn);
                const float arm = g * g * g;
                for (int k = 0; k < 3; k++) col[k] += arm_env[bin][k] * arm;
            }
            if (lid_open < 0.999f) {
                // As the eye's lids: the opening narrows toward the sides, edged with the outline's line.
                const float u = dx / RADIUS, cu = RADIUS * sqrtf(1 - u * u);
                const float edge = lid_open * (cu + LID_CLEAR), past = fabsf(dy) - edge;
                if (past > 0) col[0] = col[1] = col[2] = 0;
                const float e = fabsf(past) / scale, g = gauss(e / 0.6f);
                for (int k = 0; k < 3; k++) col[k] = col[k] * (1 - g) + line[k] * g;
            }
            *out = to565(col);
        }
    }
}

void eye_thumb_unlit(uint16_t *px, int size, uint16_t grey)
{
    const float scale = (float)(2 * RADIUS) / size;
    for (int y = 0; y < size; y++) {
        const float dy = (y + 0.5f) * scale - RADIUS;
        for (int x = 0; x < size; x++) {
            const float dx = (x + 0.5f) * scale - RADIUS;
            const float d = sqrtf(dx * dx + dy * dy);
            const bool on = fabsf(d - OUTLINE_R) < scale || fabsf(d - PUPIL_R) < scale;
            px[y * size + x] = on ? grey : 0;
        }
    }
}
