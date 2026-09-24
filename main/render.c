#include "render.h"

#include <math.h>
#include <stdbool.h>
#include <string.h>
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "config.h"

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
#define SIGMA_RING       2.2f
#define SIGMA_HALO       9.0f

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
static uint16_t lid_k[DISP_W];  // per column: 1/sqrt(1+slope^2) in Q8, turns vertical into perpendicular distance
static int16_t lid_top[DISP_W], lid_bot[DISP_W];
static bool lids_visible;
static int pupil_ix, pupil_iy;   // pupil centre, whole pixels

// Work handed to the helper core for the current strip.
static uint16_t *job_buf;
static int job_y0, job_y1;

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
static float outline_profile(float d, float amp)
{
    return amp * (expf(-0.5f * d * d / (2.5f * 2.5f)) + 0.3f * expf(-0.5f * d * d / (10.0f * 10.0f)));
}

static uint16_t to565(float r, float g, float b)
{
    int ri = (int)(fminf(r, 1.0f) * 31.0f + 0.5f);
    int gi = (int)(fminf(g, 1.0f) * 63.0f + 0.5f);
    int bi = (int)(fminf(b, 1.0f) * 31.0f + 0.5f);
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
    const float k = -0.5f / (sigma * sigma);
    for (int i = lo; i < hi; i++) {
        float d = (float)i / LUT_SCALE - center;
        float g = amp * expf(k * d * d);
        acc[i][0] += g * rgb[0];
        acc[i][1] += g * rgb[1];
        acc[i][2] += g * rgb[2];
    }
}

static void prepare(const eye_params_t *p)
{
    const float I = p->intensity;
    const float pr = p->pupil_r;
    float c[3];

    memset(acc, 0, sizeof(acc));

    // Pupil rim: bright, slightly desaturated so it reads as the "hot" centre.
    hsv(p->hue, 0.25f, 1.0f, c);
    add_ring(pr, 2.0f, 1.0f * I, c);
    add_ring(pr, 7.0f, 0.3f * I, c);

    // Iris rings, each shifted in hue and wobbling with the music. In hype mode
    // ring_phase slides them outward: they fade in at the pupil and out past the
    // iris edge, so the flow is seamless when the phase wraps.
    const float spacing = (EYE_IRIS_RADIUS - pr) / EYE_RING_COUNT;
    const float wobble_px = 4.0f + HYPE_WOBBLE_PX * p->hype;
    for (int i = 0; i <= EYE_RING_COUNT; i++) {
        const float pos = i + p->ring_phase;  // 1..EYE_RING_COUNT when calm
        const float base = pr + spacing * pos;
        const float fade = smoothstep(pr, pr + spacing, base) *
                           (1.0f - smoothstep(EYE_IRIS_RADIUS, EYE_IRIS_RADIUS + spacing, base));
        const float ri = base + p->wobble * wobble_px * sinf(p->time_s * 6.0f + pos * 1.9f);
        const float amp = (1.0f - 0.1f * pos) * I * fade;
        hsv(p->hue + pos * HUE_RING_SPREAD_DEG, 1.0f, 1.0f, c);
        add_ring(ri, SIGMA_RING, amp, c);
        add_ring(ri, SIGMA_HALO, 0.22f * amp, c);
    }

    // Beat ripples in the complementary colour.
    hsv(p->hue + 180.0f, 0.4f, 1.0f, c);
    for (int i = 0; i < EYE_MAX_RIPPLES; i++) {
        const eye_ripple_t *r = &p->ripples[i];
        if (r->amp <= 0.0f) continue;
        add_ring(r->r, 3.5f, r->amp * (0.5f + 0.5f * I), c);
        add_ring(r->r, 12.0f, 0.25f * r->amp, c);
    }

    for (int i = 0; i < RING_LUT_N; i++) ring_lut[i] = to565(acc[i][0], acc[i][1], acc[i][2]);

    // Fixed outline ring around the whole eye.
    hsv(p->hue + EYE_RING_COUNT * HUE_RING_SPREAD_DEG, 1.0f, 1.0f, c);
    const float oamp = 0.4f + 0.6f * I;
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
    lids_visible = false;
    for (int x = 0; x < DISP_W; x++) {
        float u = (x + 0.5f - DISP_CX) / DISP_RADIUS;
        float cu2 = 1.0f - u * u;
        if (cu2 <= 0.0f) {
            lid_top[x] = DISP_H;
            lid_bot[x] = -1;
            continue;
        }
        float cu = DISP_RADIUS * sqrtf(cu2);
        float h = open * (cu + LID_CLEAR_PX);
        float slope = open * u / sqrtf(cu2);
        lid_k[x] = (uint16_t)(256.0f / sqrtf(1.0f + slope * slope));
        float top = DISP_CY - h;
        float bot = DISP_CY + h;
        lid_top[x] = (int16_t)fmaxf(-LID_GLOW_PX - 1, floorf(top));
        lid_bot[x] = (int16_t)fminf(DISP_H + LID_GLOW_PX, ceilf(bot));
        if (top > DISP_CY - cu - LID_GLOW_PX || bot < DISP_CY + cu + LID_GLOW_PX) lids_visible = true;
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

static inline uint16_t shade(uint16_t idx, uint8_t o, int x, int y, bool lids)
{
    uint16_t lid = 0;
    if (lids) {
        const int t = lid_top[x], b = lid_bot[x];
        int et = y - t, eb = b - y;
        if (et < 0) et = -et;
        if (eb < 0) eb = -eb;
        // Perpendicular distance to the lid edge, in LUT units.
        const int e = ((et < eb ? et : eb) * lid_k[x] * LUT_SCALE) >> 8;
        if (e < LID_LUT_N) lid = lid_glow_lut[e];
        if (y < t || y > b) return lid;  // under a lid: only the lid line shows
    }
    uint16_t c = ring_lut[idx];
    if (o) c = add565(c, outline_lut[o - 1]);
    return lid ? add565(c, lid) : c;
}

static IRAM_ATTR void render_rows(uint16_t *dst, int y0, int y1)
{
    const bool lids = lids_visible;
    const int ox = pupil_ix - (int)DISP_CX, oy = pupil_iy - (int)DISP_CY;
    for (int y = y0; y < y1; y++, dst += DISP_W) {
        // Work on pixel pairs with 32-bit stores; keep the span even-aligned.
        const int x0 = row_x0[y] & ~1, x1 = (row_x1[y] + 1) & ~1;
        memset(dst, 0, x0 * sizeof(uint16_t));
        memset(dst + x1, 0, (DISP_W - x1) * sizeof(uint16_t));

        const uint16_t *pm = pupil_map + (y - oy + PM_PAD) * PM_W + (x0 - ox + PM_PAD);
        const uint8_t *om = outline_map + y * DISP_W + x0;
        uint32_t *out = (uint32_t *)(dst + x0);
        for (int x = x0; x < x1; x += 2, pm += 2, om += 2) {
            const uint16_t a = shade(pm[0], om[0], x, y, lids);
            const uint16_t b = shade(pm[1], om[1], x + 1, y, lids);
            // Panel expects big-endian RGB565.
            *out++ = (uint32_t)__builtin_bswap16(a) | ((uint32_t)__builtin_bswap16(b) << 16);
        }
    }
}

static void helper_main(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        render_rows(job_buf, job_y0, job_y1);
        xSemaphoreGive(helper_done);
    }
}

static bool on_color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    BaseType_t woken = pdFALSE;
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
    ESP_RETURN_ON_FALSE(free_bufs && helper_done, ESP_ERR_NO_MEM, TAG, "no memory for semaphores");

    const esp_lcd_panel_io_callbacks_t cbs = { .on_color_trans_done = on_color_done };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(panel_io, &cbs, NULL), TAG, "callback register failed");

    ESP_RETURN_ON_ERROR(build_maps(), TAG, "no memory for distance maps");
    BaseType_t ok = xTaskCreatePinnedToCore(helper_main, "render_helper", 3072, NULL, 4, &helper_task, 0);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "helper task create failed");
    return ESP_OK;
}

void render_frame(const eye_params_t *p)
{
    prepare(p);
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
        render_rows(buf, y, y + split);
        xSemaphoreTake(helper_done, portMAX_DELAY);

        esp_lcd_panel_draw_bitmap(panel, 0, y, DISP_W, y + rows, buf);
    }
}

void render_set_brightness(int percent)
{
    bsp_display_brightness_set(percent);
}
