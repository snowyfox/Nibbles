#include "render.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "bsp/display.h"
#include "driver/gpio.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "config.h"
#include "presets.h"

static const char *TAG = "render";

// Every pixel's colour is looked up by its distance from the pupil centre, in
// half-pixel steps. The tables are rebuilt once per frame.
#define LUT_SCALE        2
#define RING_LUT_N       (400 * LUT_SCALE)
#define OUTLINE_INNER_PX 180
#define OUTLINE_INNER    ((float)OUTLINE_INNER_PX)
#define OUTLINE_LUT_N    ((DISP_W / 2 + 2 - OUTLINE_INNER_PX) * LUT_SCALE)
#define LID_GLOW_PX      30       // lid lines reach this far each side (covers the outline halo)
#define LID_LUT_N        (LID_GLOW_PX * LUT_SCALE)
#define LID_CLEAR_PX     (LID_GLOW_PX + 2)  // at lid_open = 1 the lid lines sit fully outside the display

static const preset_t *preset = &presets[0];
// Spiral presets: colour by distance (2 px bins) and position along the
// spiral. The spiral map holds, per pixel relative to the pupil,
// (distance bin << 6) | spiral phase; spinning just offsets the phase.
#define SPIRAL_DBINS     (RING_LUT_N / 4)
#define SPIRAL_PHASES    64
static uint16_t spiral_lut[SPIRAL_DBINS * SPIRAL_PHASES];
#define MAX_PRESETS      32
static uint16_t *spiral_maps[MAX_PRESETS];  // per preset, built at boot (shared when settings match)
static uint16_t *spiral_map;                // the current preset's
static bool spiral_on;
static uint32_t spin_q;  // phase offset, 0..SPIRAL_PHASES-1
static float spin_turns, spin_last_t, spin_rate;
static bool spin_restart = true;  // a spiral preset was just selected
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t panel_io;
static uint16_t *bufs[STRIP_BUFFERS];
static int next_buf;
static SemaphoreHandle_t free_bufs;
static SemaphoreHandle_t helper_done;
static TaskHandle_t helper_task;

// Per-frame tables, RGB565 in CPU byte order.
static float acc[RING_LUT_N][3];
static uint16_t ring_lut[RING_LUT_N];
static uint16_t outline_lut[OUTLINE_LUT_N];
static uint16_t lid_glow_lut[LID_LUT_N];
static uint16_t lid_k[DISP_W];
static int16_t lid_gv[DISP_W];  // per column: how far (vertically) the lid glow reaches from each edge  // per column: 1/sqrt(1+slope^2) in Q8, turns vertical into perpendicular distance
static int16_t lid_top[DISP_W], lid_bot[DISP_W];
static bool lids_visible;
static int lid_row0, lid_row1;  // rows outside this range are entirely under the lids

// Tearing avoidance. The panel refreshes from its own memory at ~59 Hz and
// pulses TE high for ~0.6 ms of blanking; scanning starts on the falling edge.
// Sending a frame takes longer than one refresh, so we start at a falling
// edge: the first refresh then stays ahead of our writes (showing the whole
// old frame) and the second finds them complete (the whole new frame), as
// long as the frame is sent within about two refreshes.
#define TE_GPIO          GPIO_NUM_13
#define TE_PERIOD_US     16900
#define TE_DEADLINE_US   (2 * TE_PERIOD_US - 700)
static SemaphoreHandle_t te_sem;
static volatile int64_t te_time_us, last_done_us;
static int64_t frame_te_us;
static int late_frames;
static int64_t worst_send_us, worst_prep_us;
static int pupil_ix, pupil_iy;   // pupil centre, whole pixels

// Work handed to the helper core for the current strip.
static uint16_t *job_buf;
static int job_y0, job_y1;
static render_rows_fn job_fn;  // what the helper core draws with this frame

static void hsv(float h, float s, float v, float out[3])
{
    h = fmodf(h, 360.0f);
    if (h < 0.0f) h += 360.0f;
    float c = v * s, x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f)), m = v - c;
    float r, g, b;
    if (h < 60)       { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else              { r = c; g = 0; b = x; }
    out[0] = r + m; out[1] = g + m; out[2] = b + m;
}

static float smoothstep(float e0, float e1, float x)
{
    float t = fminf(fmaxf((x - e0) / (e1 - e0), 0.0f), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// Cross-section of the eye's neon outline: bright core plus a soft halo.
// exp(-x^2/2) for x in 0..4 in 1/64 steps: expf() is a slow software routine
// here and the glow tables use thousands of values per frame.
#define GAUSS_STEPS 64
static float gauss_tab[4 * GAUSS_STEPS + 2];

static void gauss_init(void)
{
    for (int i = 0; i < (int)(sizeof(gauss_tab) / sizeof(gauss_tab[0])); i++) {
        const float x = (float)i / GAUSS_STEPS;
        gauss_tab[i] = expf(-0.5f * x * x);
    }
}

// exp(-x^2/2), linearly interpolated; 0 beyond x = 4.
static inline float gauss(float x)
{
    x = fabsf(x) * GAUSS_STEPS;
    const int i = (int)x;
    if (i >= 4 * GAUSS_STEPS) return 0.0f;
    const float f = x - i;
    return gauss_tab[i] + (gauss_tab[i + 1] - gauss_tab[i]) * f;
}

static float outline_profile(float d, float amp)
{
    return amp * (gauss(d / 2.5f) + 0.3f * gauss(d / 10.0f));
}

static inline uint16_t to565(float r, float g, float b)
{
    int ri = (int)(r * 31.0f + 0.5f), gi = (int)(g * 63.0f + 0.5f), bi = (int)(b * 31.0f + 0.5f);
    if (ri > 31) ri = 31;
    if (gi > 63) gi = 63;
    if (bi > 31) bi = 31;
    return (uint16_t)((ri << 11) | (gi << 5) | bi);
}

static inline uint16_t add565(uint16_t a, uint16_t b)
{
    int r = (a >> 11) + (b >> 11);
    int g = ((a >> 5) & 63) + ((b >> 5) & 63);
    int bl = (a & 31) + (b & 31);
    if (r > 31) r = 31;
    if (g > 63) g = 63;
    if (bl > 31) bl = 31;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

// Add a Gaussian glow ring of the given colour to the accumulation table.
static void add_ring(float center, float sigma, float amp, const float rgb[3])
{
    if (amp <= 0.001f) return;
    int lo = (int)((center - 4.0f * sigma) * LUT_SCALE);
    int hi = (int)((center + 4.0f * sigma) * LUT_SCALE) + 1;
    if (lo < 0) lo = 0;
    if (hi > RING_LUT_N) hi = RING_LUT_N;
    const float inv = 1.0f / sigma;
    for (int i = lo; i < hi; i++) {
        float d = (float)i / LUT_SCALE - center;
        float g = amp * gauss(d * inv);
        acc[i][0] += g * rgb[0];
        acc[i][1] += g * rgb[1];
        acc[i][2] += g * rgb[2];
    }
}

// A hard-edged band of half-width hw with a 1 px soft edge.
static void add_band(float center, float hw, float amp, const float rgb[3])
{
    if (amp <= 0.001f) return;
    int lo = (int)((center - hw - 2.0f) * LUT_SCALE);
    int hi = (int)((center + hw + 2.0f) * LUT_SCALE) + 1;
    if (lo < 0) lo = 0;
    if (hi > RING_LUT_N) hi = RING_LUT_N;
    for (int i = lo; i < hi; i++) {
        const float g = amp * (1.0f - smoothstep(hw - 1.0f, hw + 1.0f, fabsf((float)i / LUT_SCALE - center)));
        acc[i][0] += g * rgb[0];
        acc[i][1] += g * rgb[1];
        acc[i][2] += g * rgb[2];
    }
}

// Colour at ring position pos for the current preset.
static void preset_color(const eye_params_t *p, float pos, float out[3])
{
    if (preset->rainbow) {
        hsv(p->hue + pos * preset->hue_spread, preset->sat, 1.0f, out);
        return;
    }
    float t = pos * preset->palette_step + p->hue / 360.0f * preset->palette_music;
    t = (t - floorf(t)) * 4.0f;
    const int i = (int)t % 4;
    const float f = t - floorf(t);
    const preset_rgb_t *a = &preset->palette[i], *b = &preset->palette[(i + 1) % 4];
    out[0] = a->r + (b->r - a->r) * f;
    out[1] = a->g + (b->g - a->g) * f;
    out[2] = a->b + (b->b - a->b) * f;
}

static void whiten(float c[3], float w)
{
    for (int k = 0; k < 3; k++) c[k] += (1.0f - c[k]) * w;
}

static void prepare(const eye_params_t *p)
{
    const float I = p->intensity * p->master;
    const float pr = p->pupil_r * preset->pupil_scale;
    const int n = preset->rings;
    float c[3];

    memset(acc, 0, sizeof(acc));

    // Pupil rim: bright, washed toward white so it reads as the "hot" centre.
    preset_color(p, 0.0f, c);
    whiten(c, preset->rim_white);
    add_ring(pr, 2.0f, preset->rim_amp * I, c);
    add_ring(pr, 7.0f, 0.3f * preset->rim_amp * I, c);

    // Soft glow filling the iris.
    if (preset->fill > 0.0f) {
        const int lo = (int)(pr * LUT_SCALE), hi = (int)((EYE_IRIS_RADIUS + 10.0f) * LUT_SCALE);
        for (int i = lo; i < hi && i < RING_LUT_N; i++) {
            const float d = (float)i / LUT_SCALE;
            const float t = (d - pr) / (EYE_IRIS_RADIUS - pr);
            const float g = preset->fill * I * smoothstep(0.0f, 0.15f, t) * (1.0f - smoothstep(0.9f, 1.05f, t));
            preset_color(p, t * n, c);
            acc[i][0] += g * c[0];
            acc[i][1] += g * c[1];
            acc[i][2] += g * c[2];
        }
    }

    // Iris rings, wobbling with the music. They drift outward (or inward) at
    // the preset's flow rate, and faster in hype mode (ring_phase); they fade
    // in at the pupil and out past the iris edge, so the flow is seamless.
    float phase = p->ring_phase + p->time_s * preset->flow;
    phase -= floorf(phase);
    const float spacing = n > 0 ? (EYE_IRIS_RADIUS - pr) / n : 0.0f;
    const float wobble_px = preset->wobble_px + HYPE_WOBBLE_PX * p->hype;
    for (int i = 0; n > 0 && i <= n; i++) {
        const float pos = i + phase;  // 1..n when calm
        const float base = pr + spacing * pos;
        const float fade = smoothstep(pr, pr + spacing, base) *
                           (1.0f - smoothstep(EYE_IRIS_RADIUS, EYE_IRIS_RADIUS + spacing, base));
        const float ri = base + p->wobble * wobble_px * sinf(p->time_s * 6.0f + pos * 1.9f);
        const float amp = (1.0f - 0.5f * pos / n) * I * fade;
        preset_color(p, pos, c);
        if (preset->bands) add_band(ri, preset->ring_width, amp, c);
        else add_ring(ri, preset->ring_width, amp, c);
        add_ring(ri, preset->halo_sigma, preset->halo_amp * amp, c);
    }

    // Beat ripples.
    if (preset->rainbow) hsv(p->hue + preset->ripple_pos, 1.0f, 1.0f, c);
    else preset_color(p, preset->ripple_pos, c);
    whiten(c, preset->ripple_white);
    for (int i = 0; i < EYE_MAX_RIPPLES; i++) {
        const eye_ripple_t *r = &p->ripples[i];
        if (r->amp <= 0.0f) continue;
        add_ring(r->r, 3.5f, r->amp * (0.5f + 0.5f * I), c);
        add_ring(r->r, 12.0f, 0.25f * r->amp, c);
    }

    for (int i = 0; i < RING_LUT_N; i++) ring_lut[i] = to565(acc[i][0], acc[i][1], acc[i][2]);

    // Spiral presets: arms twisting out from the pupil, spinning in time with
    // the music, over the same pupil rim, fill and beat ripples.
    spiral_on = preset->spiral_arms > 0 && spiral_map;
    if (spiral_on) {
        const float target = preset_spin_rate(preset, p->tempo_bpm, p->hype);
        if (spin_restart) {
            // Start at this preset's own speed, not whatever the last spiral
            // preset was doing when it left, and don't count the time since.
            spin_rate = target;
            spin_last_t = p->time_s;
            spin_restart = false;
        }
        const float dt = fminf(0.1f, fmaxf(0.0f, p->time_s - spin_last_t));
        spin_last_t = p->time_s;
        // Ease toward the tempo-locked speed so tempo changes (and the odd
        // jittery tempo estimate) speed it up and slow it down smoothly.
        spin_rate += (target - spin_rate) * (1.0f - expf(-dt / SPIN_EASE_S));
        spin_turns += dt * spin_rate;
        spin_turns -= floorf(spin_turns);
        // The spiral map repeats once per arm, so a full turn is `arms` phase
        // periods. Subtracting the spin turns the arms the other way round.
        const float periods = spin_turns * preset->spiral_arms;
        spin_q = (uint32_t)(SPIRAL_PHASES - (int)((periods - floorf(periods)) * SPIRAL_PHASES)) & (SPIRAL_PHASES - 1);
        // Arm brightness across one spiral period (0..256), narrow bright arms.
        static int arm[SPIRAL_PHASES];
        if (!arm[0]) {
            for (int q = 0; q < SPIRAL_PHASES; q++) {
                const float g = 0.5f + 0.5f * cosf(2.0f * (float)M_PI * (q + 0.5f) / SPIRAL_PHASES);
                arm[q] = (int)(g * g * g * 256.0f + 0.5f);
            }
        }
        // Integer maths: 12800 entries per frame.
        for (int b = 0; b < SPIRAL_DBINS; b++) {
            const float d = (float)b * 4.0f / LUT_SCALE + 1.0f;
            const float env = I * smoothstep(pr, pr + 15.0f, d) *
                              (1.0f - smoothstep(EYE_IRIS_RADIUS + 20.0f, EYE_OUTLINE_RADIUS - 10.0f, d)) *
                              (1.0f - 0.3f * d / EYE_OUTLINE_RADIUS);
            preset_color(p, d / 40.0f, c);
            const float *r = acc[b * 4 + 2];
            const int rr = (int)(r[0] * 31.0f * 256.0f), rg = (int)(r[1] * 63.0f * 256.0f), rb = (int)(r[2] * 31.0f * 256.0f);
            const int cr = (int)(env * c[0] * 31.0f * 256.0f), cg = (int)(env * c[1] * 63.0f * 256.0f),
                      cb = (int)(env * c[2] * 31.0f * 256.0f);
            uint16_t *out = &spiral_lut[b * SPIRAL_PHASES];
            for (int q = 0; q < SPIRAL_PHASES; q++) {
                int R = (rr + ((cr * arm[q]) >> 8) + 128) >> 8;
                int G = (rg + ((cg * arm[q]) >> 8) + 128) >> 8;
                int B = (rb + ((cb * arm[q]) >> 8) + 128) >> 8;
                if (R > 31) R = 31;
                if (G > 63) G = 63;
                if (B > 31) B = 31;
                out[q] = (uint16_t)((R << 11) | (G << 5) | B);
            }
        }
    }

    // Fixed outline ring around the whole eye.
    preset_color(p, n, c);
    // Always at full brightness, whatever the music, hype or sleep is doing
    // (only a blackout bump dims it).
    const float oamp = p->master;
    for (int i = 0; i < OUTLINE_LUT_N; i++) {
        float g = outline_profile(OUTLINE_INNER + (float)i / LUT_SCALE - EYE_OUTLINE_RADIUS, oamp);
        outline_lut[i] = to565(g * c[0], g * c[1], g * c[2]);
    }

    // Eyelid edges use the same line as the outline, so the eye keeps one
    // consistent outline as it closes (including when asleep).
    for (int i = 0; i < LID_LUT_N; i++) {
        float g = outline_profile((float)i / LUT_SCALE, oamp);
        lid_glow_lut[i] = to565(g * c[0], g * c[1], g * c[2]);
    }

    // Eyelids: per-column top and bottom limits.
    const float open = p->lid_open;
    lids_visible = open < 0.999f;  // fully (or extra-wide) open: no lids at all
    lid_row0 = DISP_H;
    lid_row1 = -1;
    for (int x = 0; lids_visible && x < DISP_W; x++) {
        float u = (x + 0.5f - DISP_CX) / DISP_RADIUS;
        float cu2 = 1.0f - u * u;
        if (cu2 <= 0.0f) {
            lid_top[x] = DISP_H;
            lid_bot[x] = -1;
            lid_gv[x] = 0;
            continue;
        }
        float cu = DISP_RADIUS * sqrtf(cu2);
        float h = open * (cu + LID_CLEAR_PX);
        float slope = open * u / sqrtf(cu2);
        const float k = 1.0f / sqrtf(1.0f + slope * slope);
        lid_k[x] = (uint16_t)(256.0f * k);
        // Glow reaches LID_GLOW_PX perpendicular to the edge, further vertically where it is steep.
        lid_gv[x] = (int16_t)fminf(400.0f, ceilf(LID_GLOW_PX / k) + 1.0f);
        lid_top[x] = (int16_t)floorf(DISP_CY - h);
        lid_bot[x] = (int16_t)ceilf(DISP_CY + h);
        // Only the on-screen part of the column matters when choosing rows to skip.
        const int r0 = (int)fmaxf(lid_top[x] - lid_gv[x], DISP_CY - cu - 1.0f);
        const int r1 = (int)fminf(lid_bot[x] + lid_gv[x], DISP_CY + cu + 1.0f);
        if (r0 < lid_row0) lid_row0 = r0;
        if (r1 > lid_row1) lid_row1 = r1;
    }

    const float lim = EYE_MAX_LOOK_PX;
    pupil_ix = (int)lroundf(DISP_CX + fmaxf(-lim, fminf(lim, p->pupil_x)));
    pupil_iy = (int)lroundf(DISP_CY + fmaxf(-lim, fminf(lim, p->pupil_y)));
}

// Distance maps, built once at boot. The pupil map covers the display plus the
// full look range, so moving the pupil only changes where rows are read from.
#define PM_PAD   ((int)EYE_MAX_LOOK_PX + 1)
#define PM_W     (DISP_W + 2 * PM_PAD)
#define PM_H     (DISP_H + 2 * PM_PAD)
static uint16_t *pupil_map;   // ring_lut index for each position relative to the pupil
static uint8_t *outline_map;  // 0 = no outline, else 1 + outline_lut index

static int16_t row_x0[DISP_H], row_x1[DISP_H];  // visible span of each row

static esp_err_t build_maps(void)
{
    pupil_map = heap_caps_malloc(PM_W * PM_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    outline_map = heap_caps_malloc(DISP_W * DISP_H, MALLOC_CAP_SPIRAM);
    if (!pupil_map || !outline_map) return ESP_ERR_NO_MEM;

    for (int y = 0; y < PM_H; y++) {
        const float dy = y + 0.5f - (PM_PAD + DISP_CY);
        for (int x = 0; x < PM_W; x++) {
            const float dx = x + 0.5f - (PM_PAD + DISP_CX);
            int i = (int)(sqrtf(dx * dx + dy * dy) * LUT_SCALE);
            pupil_map[y * PM_W + x] = (uint16_t)(i < RING_LUT_N ? i : RING_LUT_N - 1);
        }
    }
    for (int y = 0; y < DISP_H; y++) {
        const float fy = y + 0.5f - DISP_CY;
        const float h2 = DISP_RADIUS * DISP_RADIUS - fy * fy;
        if (h2 <= 0.0f) {
            row_x0[y] = row_x1[y] = 0;
        } else {
            const float half = sqrtf(h2);
            row_x0[y] = (int16_t)fmaxf(0.0f, DISP_CX - half);
            row_x1[y] = (int16_t)fminf(DISP_W, ceilf(DISP_CX + half));
        }
        for (int x = 0; x < DISP_W; x++) {
            const float ox = x + 0.5f - DISP_CX;
            const float d0 = sqrtf(ox * ox + fy * fy);
            const int j = (int)((d0 - OUTLINE_INNER) * LUT_SCALE);
            outline_map[y * DISP_W + x] = (d0 < OUTLINE_INNER || j >= OUTLINE_LUT_N - 1) ? 0 : (uint8_t)(j + 1);
        }
    }
    return ESP_OK;
}

static inline uint16_t base_color(uint16_t idx, uint8_t o)
{
    uint16_t c = ring_lut[idx];
    return o ? add565(c, outline_lut[o - 1]) : c;
}

static inline uint16_t spiral_color(uint16_t sm, uint8_t o)
{
    const uint16_t c = spiral_lut[(sm & ~(SPIRAL_PHASES - 1)) | ((sm + spin_q) & (SPIRAL_PHASES - 1))];
    return o ? add565(c, outline_lut[o - 1]) : c;
}

// Colour of a pixel that may be near or under a lid, given its unlidded colour.
static inline uint16_t shade_lid(uint16_t base, int x, int y)
{
    const int t = lid_top[x], b = lid_bot[x];
    int et = y - t, eb = b - y;
    if (et < 0) et = -et;
    if (eb < 0) eb = -eb;
    // Perpendicular distance to the nearest lid edge, in LUT units.
    const int e = ((et < eb ? et : eb) * lid_k[x] * LUT_SCALE) >> 8;
    const uint16_t lid = e < LID_LUT_N ? lid_glow_lut[e] : 0;
    if (y < t || y > b) return lid;  // under a lid: only the lid line shows
    return lid ? add565(base, lid) : base;
}

static inline bool lid_deep(int x, int y)      // under a lid, beyond the lid line's glow
{
    return y < lid_top[x] - lid_gv[x] || y > lid_bot[x] + lid_gv[x];
}

static inline bool lid_clear(int x, int y)     // between the lids, beyond the lid line's glow
{
    return y > lid_top[x] + lid_gv[x] && y < lid_bot[x] - lid_gv[x];
}

// Pixel pairs over [xa, xb) (both even); 32-bit stores, big-endian RGB565 for
// the panel. map is the pupil map, or the spiral map for spiral presets.
#define PAIR(a, b) ((uint32_t)__builtin_bswap16(a) | ((uint32_t)__builtin_bswap16(b) << 16))

static inline void span_plain(uint16_t *dst, const uint16_t *map, bool spiral, const uint8_t *om, int xa, int xb)
{
    uint32_t *out = (uint32_t *)(dst + xa);
    if (spiral) {
        for (int x = xa; x < xb; x += 2, map += 2, om += 2)
            *out++ = PAIR(spiral_color(map[0], om[0]), spiral_color(map[1], om[1]));
    } else {
        for (int x = xa; x < xb; x += 2, map += 2, om += 2)
            *out++ = PAIR(base_color(map[0], om[0]), base_color(map[1], om[1]));
    }
}

static inline void span_lid(uint16_t *dst, const uint16_t *map, bool spiral, const uint8_t *om, int xa, int xb, int y)
{
    uint32_t *out = (uint32_t *)(dst + xa);
    for (int x = xa; x < xb; x += 2, map += 2, om += 2) {
        const uint16_t a = spiral ? spiral_color(map[0], om[0]) : base_color(map[0], om[0]);
        const uint16_t b = spiral ? spiral_color(map[1], om[1]) : base_color(map[1], om[1]);
        *out++ = PAIR(shade_lid(a, x, y), shade_lid(b, x + 1, y));
    }
}

// Build a spiral map for the given arms and twist (~0.74 MB of PSRAM, takes a
// moment, so done at boot).
static uint16_t *build_spiral_map(int arms, float twist_px)
{
    uint16_t *map = heap_caps_malloc(PM_W * PM_H * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!map) return NULL;
    for (int y = 0; y < PM_H; y++) {
        const float dy = y + 0.5f - (PM_PAD + DISP_CY);
        for (int x = 0; x < PM_W; x++) {
            const float dx = x + 0.5f - (PM_PAD + DISP_CX);
            const float d = sqrtf(dx * dx + dy * dy);
            const int bin = (int)fminf(d * LUT_SCALE / 4.0f, SPIRAL_DBINS - 1);
            float turn = atan2f(dy, dx) / (2.0f * (float)M_PI) * arms + d / twist_px;
            turn -= floorf(turn);
            map[y * PM_W + x] = (uint16_t)((bin << 6) | ((int)(turn * SPIRAL_PHASES) & (SPIRAL_PHASES - 1)));
        }
    }
    return map;
}

static IRAM_ATTR void render_rows(uint16_t *dst, int y0, int y1)
{
    const bool lids = lids_visible;
    const int ox = pupil_ix - (int)DISP_CX, oy = pupil_iy - (int)DISP_CY;
    for (int y = y0; y < y1; y++, dst += DISP_W) {
        if (lids && (y < lid_row0 || y > lid_row1)) {
            memset(dst, 0, DISP_W * sizeof(uint16_t));
            continue;
        }
        // Work on pixel pairs; keep the span even-aligned.
        const int x0 = row_x0[y] & ~1, x1 = (row_x1[y] + 1) & ~1;
        memset(dst, 0, x0 * sizeof(uint16_t));
        memset(dst + x1, 0, (DISP_W - x1) * sizeof(uint16_t));

        const bool sp = spiral_on;
        const uint16_t *pm = (sp ? spiral_map : pupil_map) + (y - oy + PM_PAD) * PM_W + (x0 - ox + PM_PAD) - x0;
        const uint8_t *om = outline_map + y * DISP_W;
        if (!lids) {
            span_plain(dst, pm + x0, sp, om + x0, x0, x1);
            continue;
        }

        // Split the row so only pixels near a lid edge pay for the lid maths:
        // [x0,bl) black | [bl,ia) lid | [ia,ib) clear | [ib,br) lid | [br,x1) black.
        // Each boundary is found by scanning from its own side, so every
        // shortcut region is exact; anything uncertain gets the lid path.
        int bl = x0, br = x1;
        while (bl < x1 && lid_deep(bl, y)) bl++;
        while (br > bl && lid_deep(br - 1, y)) br--;
        bl &= ~1;
        br = (br + 1) & ~1;
        if (bl >= br) {
            memset(dst + x0, 0, (x1 - x0) * sizeof(uint16_t));
            continue;
        }
        const int c = (DISP_W / 2) & ~1;
        int ia = c, ib = c;
        if (c >= bl && c < br && lid_clear(c, y)) {
            while (ib < br && lid_clear(ib, y)) ib++;
            while (ia > bl && lid_clear(ia - 1, y)) ia--;
            ia = (ia + 1) & ~1;
            ib &= ~1;
            if (ia > ib) ia = ib;
        } else {
            ia = ib = bl;  // no clear stretch: lid path for the whole visible part
        }

        memset(dst + x0, 0, (bl - x0) * sizeof(uint16_t));
        span_lid(dst, pm + bl, sp, om + bl, bl, ia, y);
        span_plain(dst, pm + ia, sp, om + ia, ia, ib);
        span_lid(dst, pm + ib, sp, om + ib, ib, br, y);
        memset(dst + br, 0, (x1 - br) * sizeof(uint16_t));
    }
}

static void helper_main(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        job_fn(job_buf, job_y0, job_y1);
        xSemaphoreGive(helper_done);
    }
}

static void IRAM_ATTR on_te(void *arg)
{
    BaseType_t woken = pdFALSE;
    te_time_us = esp_timer_get_time();
    xSemaphoreGiveFromISR(te_sem, &woken);
    if (woken) portYIELD_FROM_ISR();
}

static bool on_color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    BaseType_t woken = pdFALSE;
    last_done_us = esp_timer_get_time();
    xSemaphoreGiveFromISR(free_bufs, &woken);
    return woken == pdTRUE;
}

esp_err_t render_init(void)
{
    const bsp_display_config_t cfg = {
        .max_transfer_sz = DISP_W * STRIP_ROWS * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(bsp_display_new(&cfg, &panel, &panel_io), TAG, "display init failed");

    for (int i = 0; i < STRIP_BUFFERS; i++) {
        bufs[i] = heap_caps_malloc(DISP_W * STRIP_ROWS * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        ESP_RETURN_ON_FALSE(bufs[i], ESP_ERR_NO_MEM, TAG, "no memory for strip buffer");
    }
    free_bufs = xSemaphoreCreateCounting(STRIP_BUFFERS, STRIP_BUFFERS);
    helper_done = xSemaphoreCreateBinary();
    te_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(te_sem, ESP_ERR_NO_MEM, TAG, "no memory for semaphores");
    const gpio_config_t te_cfg = {
        .pin_bit_mask = 1ULL << TE_GPIO,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&te_cfg), TAG, "TE pin config failed");
    esp_err_t isr_err = gpio_install_isr_service(0);
    ESP_RETURN_ON_FALSE(isr_err == ESP_OK || isr_err == ESP_ERR_INVALID_STATE, isr_err, TAG, "GPIO ISR service failed");
    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(TE_GPIO, on_te, NULL), TAG, "TE interrupt failed");
    ESP_RETURN_ON_FALSE(free_bufs && helper_done, ESP_ERR_NO_MEM, TAG, "no memory for semaphores");

    const esp_lcd_panel_io_callbacks_t cbs = { .on_color_trans_done = on_color_done };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, NULL), TAG, "callback register failed");

    gauss_init();
    ESP_RETURN_ON_ERROR(build_maps(), TAG, "no memory for distance maps");
    // Build spiral maps now rather than freezing the eye the first time a
    // spiral preset comes up. Presets with the same arms and twist share one.
    ESP_RETURN_ON_FALSE(preset_count <= MAX_PRESETS, ESP_ERR_INVALID_SIZE, TAG, "too many presets");
    for (int i = 0; i < preset_count; i++) {
        if (presets[i].spiral_arms <= 0) continue;
        for (int j = 0; j < i && !spiral_maps[i]; j++) {
            if (presets[j].spiral_arms == presets[i].spiral_arms && presets[j].spiral_twist == presets[i].spiral_twist)
                spiral_maps[i] = spiral_maps[j];
        }
        if (!spiral_maps[i]) spiral_maps[i] = build_spiral_map(presets[i].spiral_arms, presets[i].spiral_twist);
        if (!spiral_maps[i]) ESP_LOGE(TAG, "no memory for the %s spiral; it will show plain rings", presets[i].name);
    }
    BaseType_t ok = xTaskCreatePinnedToCore(helper_main, "render_helper", 3072, NULL, 7, &helper_task, 0);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "helper task create failed");
    return ESP_OK;
}

static void send_frame(render_rows_fn draw);

void render_frame(const eye_params_t *p)
{
    const int64_t t0 = esp_timer_get_time();
    prepare(p);
    const int64_t prep_us = esp_timer_get_time() - t0;
    if (prep_us > worst_prep_us) worst_prep_us = prep_us;
    send_frame(render_rows);
}

void render_frame_rows(render_rows_fn rows)
{
    send_frame(rows);
}

// Wait for the panel's refresh edge, then draw and send the frame strip by
// strip, both cores drawing half of each strip.
static void send_frame(render_rows_fn draw)
{
    job_fn = draw;

    // Check the previous frame made its deadline, then wait for a fresh TE
    // falling edge. The timeout keeps things running if TE ever stops.
    if (frame_te_us) {
        const int64_t send_us = last_done_us - frame_te_us;
        if (send_us > TE_DEADLINE_US) late_frames++;
        if (send_us > worst_send_us) worst_send_us = send_us;
    }
    xSemaphoreTake(te_sem, 0);
    if (xSemaphoreTake(te_sem, pdMS_TO_TICKS(50)) == pdTRUE) frame_te_us = te_time_us;
    else frame_te_us = 0;

    for (int y = 0; y < DISP_H; y += STRIP_ROWS) {
        const int rows = DISP_H - y < STRIP_ROWS ? DISP_H - y : STRIP_ROWS;
        const int split = rows / 2;
        xSemaphoreTake(free_bufs, portMAX_DELAY);
        uint16_t *buf = bufs[next_buf];
        next_buf = (next_buf + 1) % STRIP_BUFFERS;

        // Helper core renders the bottom half of the strip while this core does the top.
        job_buf = buf + split * DISP_W;
        job_y0 = y + split;
        job_y1 = y + rows;
        xTaskNotifyGive(helper_task);
        draw(buf, y, y + split);
        xSemaphoreTake(helper_done, portMAX_DELAY);

        esp_lcd_panel_draw_bitmap(panel, 0, y, DISP_W, y + rows, buf);
    }
}

void render_take_stats(int *late, float *worst_ms, float *worst_prep_ms)
{
    *late = late_frames;
    *worst_ms = worst_send_us / 1000.0f;
    *worst_prep_ms = worst_prep_us / 1000.0f;
    late_frames = 0;
    worst_send_us = worst_prep_us = 0;
}

void render_set_preset(int index)
{
    index %= preset_count;
    preset = &presets[index];
    spiral_map = spiral_maps[index];
    spin_restart = true;
}

void render_set_brightness(int percent)
{
    bsp_display_brightness_set(percent);
}
