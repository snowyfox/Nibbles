#include "cartoon.h"

#include <math.h>
#include <string.h>
#include "config.h"

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define CARTOON_FAST IRAM_ATTR
#else
#define CARTOON_FAST
#endif

#define W   DISP_W
#define H   DISP_H
#define CX  ((int)DISP_CX)
#define CY  ((int)DISP_CY)
#define PI_F 3.14159265f

// ------------------------------------------------------------------ helpers

// RGB565, byte-swapped for the panel.
static inline uint16_t rgb(int r, int g, int b)
{
    const uint16_t c = (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    return (uint16_t)((c >> 8) | (c << 8));
}

static inline uint16_t mix(const int a[3], const int b[3], float t)
{
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return rgb((int)(a[0] + (b[0] - a[0]) * t), (int)(a[1] + (b[1] - a[1]) * t), (int)(a[2] + (b[2] - a[2]) * t));
}

static inline int iabs(int v) { return v < 0 ? -v : v; }

// Visible span of each row on the round panel.
static int16_t row_x0[H], row_x1[H];

// Distance from the centre in quarter pixels, and angle (ANG_STEPS per turn,
// 0 = right, a quarter turn = down), for every pixel.
#define ANG_STEPS 1024
#define ANG_MASK  (ANG_STEPS - 1)
static uint16_t *dist4;
static uint16_t *ang;

// The animations, in cycle order.
enum { A_SPARKLE, A_HYPNOTOAD, A_SHARINGAN, A_MANGEKYO, A_HAPPY };
static const char *const names[CARTOON_COUNT] = { "Sparkle", "Hypnotoad", "Sharingan", "Mangekyo Sharingan", "Happy" };

// ------------------------------------------------------------------ per-frame state
// Written by cartoon_prepare, read by both cores in cartoon_rows.

static int anim_now;
static const uint16_t BLACK_PX = 0;

// Sparkle (anime eye)
static struct {
    int icx, icy, r2, rin2, ring_lo2, ring_hi2, pr2, pout2;
    int h1x, h1y, h1r2, h2x, h2y, h2r2, h3x, h3y, h3r2;
    int16_t lid_top[W], lid_bot[W];
    uint16_t iris[H], ring[H], sclera[H];
    uint16_t outline, pupil, white, lash;
} sp;

// Hypnotoad: a glossy green toad eye that, from its black pupil, fills with
// oscillating multicoloured rings (red, then yellow, and on round the
// colours), under heavy lids of olive-brown skin.
static struct {
    uint16_t palette[256], palette_dim[256];  // one ring cycle; dimmer copy for the dome's edge
    int k, phase;                    // palette index = (dist4 * k >> 8) - phase
    int front4;                      // rings reach this far (quarter pixels); green beyond
    int pupil4, dim4, eye4, rim4;
    int16_t lid[W], lid_lo[W];       // upper and lower lid edges per column
    uint16_t skin[H], green[240], lid_line, black;
    int gx, gy, grx2, gry2;          // glint ellipse
    uint16_t glint;
} ht;

// Sharingan (both kinds): red iris, black pupil and marks, a glint.
typedef struct { int x, y, r2; } glint_t;
static struct {
    uint16_t iris[240], outline, black, white;
    int edge4, pupil4, ring4, ring_w4;
    // Classic: three tomoe as intervals along each angle step: black where
    // head_lo <= d < head_hi, or |d - tail_c| < tail_w (quarter pixels).
    uint16_t head_lo[ANG_STEPS], head_hi[ANG_STEPS], tail_c[ANG_STEPS], tail_w[ANG_STEPS];
    // Mangekyo: black core, and three curved blades: at radius r (pixels) a
    // blade covers blade_w[r] angle steps after its edge, which curves by
    // twist[r] steps.
    int core4, rot;
    uint16_t blade_w[240], twist[240];
    glint_t glint[2];
} sh;

// A four-point sparkle: centre and arm length (0 = hidden).
typedef struct { int x, y, len; } sparkle_t;

// Happy (^ eye)
static struct {
    int acx, acy, ro2, ri2, oo2, oi2, cap2, capo2, lx, rx;
    int bx, by, brx2, bry2;          // blush ellipse
    uint16_t bg[H], cream, outline, blush;
    sparkle_t sparks[2];
    uint16_t spark;
} hp;

// ------------------------------------------------------------------ init

bool cartoon_init(void *(*alloc)(size_t size))
{
    dist4 = alloc((size_t)W * H * sizeof(uint16_t));
    ang = alloc((size_t)W * H * sizeof(uint16_t));
    if (!dist4 || !ang) return false;
    for (int y = 0; y < H; y++) {
        const float fy = y + 0.5f - DISP_CY;
        const float h2 = DISP_RADIUS * DISP_RADIUS - fy * fy;
        if (h2 <= 0.0f) {
            row_x0[y] = row_x1[y] = 0;
        } else {
            const float half = sqrtf(h2);
            row_x0[y] = (int16_t)fmaxf(0.0f, DISP_CX - half);
            row_x1[y] = (int16_t)fminf(W, ceilf(DISP_CX + half));
        }
        for (int x = 0; x < W; x++) {
            const float fx = x + 0.5f - DISP_CX;
            dist4[y * W + x] = (uint16_t)(sqrtf(fx * fx + fy * fy) * 4.0f);
            float a = atan2f(fy, fx) / (2.0f * PI_F);
            if (a < 0.0f) a += 1.0f;
            ang[y * W + x] = (uint16_t)((int)(a * ANG_STEPS + 0.5f) & ANG_MASK);
        }
    }
    return true;
}

const char *cartoon_name(int anim)
{
    return anim >= 0 && anim < CARTOON_COUNT ? names[anim] : "?";
}

// ------------------------------------------------------------------ prepare

static void prep_sparkle(float t, int ms)
{
    static const int iris_top[3] = { 38, 18, 110 }, iris_bot[3] = { 80, 210, 255 };
    static const int sclera_top[3] = { 196, 188, 222 }, sclera_mid[3] = { 250, 248, 255 };
    const int r = 150;
    sp.icx = CX + (int)(ms * 22.0f * sinf(0.37f * t));
    sp.icy = CY + 8 + (int)(10.0f * sinf(0.23f * t + 1.0f));
    sp.r2 = r * r;
    sp.rin2 = (r - 8) * (r - 8);
    sp.ring_hi2 = (int)(0.80f * r) * (int)(0.80f * r);
    sp.ring_lo2 = (int)(0.72f * r) * (int)(0.72f * r);
    const int pr = 58 + (int)(3.0f * sinf(1.3f * t));
    sp.pr2 = pr * pr;
    sp.pout2 = (pr + 5) * (pr + 5);
    const float tw = 0.5f + 0.5f * sinf(2.2f * t);
    const int h1r = 32 + (int)(5.0f * tw), h2r = 13 + (int)(3.0f * (1.0f - tw)), h3r = 7;
    sp.h1x = sp.icx - ms * 52; sp.h1y = sp.icy - 56; sp.h1r2 = h1r * h1r;
    sp.h2x = sp.icx + ms * 50; sp.h2y = sp.icy + 44; sp.h2r2 = h2r * h2r;
    sp.h3x = sp.icx - ms * 18; sp.h3y = sp.icy - 92; sp.h3r2 = h3r * h3r;
    for (int y = 0; y < H; y++) {
        sp.iris[y] = mix(iris_top, iris_bot, (y - (sp.icy - r)) / (2.0f * r) * 1.15f);
        sp.ring[y] = mix(iris_top, iris_bot, (y - (sp.icy - r)) / (2.0f * r) * 1.15f + 0.35f);
        sp.sclera[y] = mix(sclera_top, sclera_mid, (y - 20) / 170.0f);
    }
    sp.outline = rgb(24, 12, 48);
    sp.pupil = rgb(14, 8, 30);
    sp.white = rgb(255, 255, 255);
    sp.lash = rgb(30, 14, 40);

    // Blink every 4 s: lids close and open over 0.3 s.
    const float cycle = fmodf(t, 4.0f), dur = 0.3f;
    const float close = cycle < dur ? sinf(PI_F * cycle / dur) : 0.0f;
    const float open_top = 205.0f * (1.0f - close), open_bot = 190.0f * (1.0f - close);
    for (int x = 0; x < W; x++) {
        const float u = (x - CX) / 233.0f;
        sp.lid_top[x] = (int16_t)(CY - open_top + 40.0f * u * u * (1.0f - close));
        sp.lid_bot[x] = (int16_t)(CY + open_bot - 25.0f * u * u * (1.0f - close));
    }
}

static void prep_hypnotoad(float t, int ms)
{
    // Ring colours, pupil outward: red, yellow, green, cyan, blue, magenta,
    // with a thin dark line between bands so the rings stay crisp.
    static const int hues[6][3] = { { 235, 20, 20 },  { 255, 225, 30 }, { 40, 210, 60 },
                                    { 30, 220, 230 }, { 40, 80, 245 },  { 215, 40, 220 } };
    for (int i = 0; i < 256; i++) {
        const int band = i * 6 / 256, pos = (i * 6) % 256;
        const int *c = hues[band];
        const bool line = pos > 236;  // the last few steps of each band
        ht.palette[i] = line ? rgb(20, 10, 20) : rgb(c[0], c[1], c[2]);
        ht.palette_dim[i] = line ? rgb(12, 6, 12) : rgb(c[0] * 3 / 5, c[1] * 3 / 5, c[2] * 3 / 5);
    }
    // Every 8 s the eye rests green for a moment, then the rings bloom out.
    const float cycle = fmodf(t, 8.0f), rest = 0.9f;
    ht.front4 = cycle < rest ? 0 : (int)(fminf((cycle - rest) * 320.0f, 240.0f) * 4.0f);
    const float spacing = 150.0f;                        // pixels for all six colours
    ht.k = (int)(256.0f * 256.0f / (spacing * 4.0f));    // dist4 -> palette steps, << 8
    // Flowing outward and oscillating in and out as it goes.
    ht.phase = (int)((t * 0.9f + 0.12f * sinf(t * 7.0f)) * 256.0f);
    ht.pupil4 = (int)((22.0f + 4.0f * sinf(t * 3.0f)) * 4.0f);
    ht.dim4 = 170 * 4;
    ht.eye4 = 204 * 4;
    ht.rim4 = 211 * 4;
    static const int g_in[3] = { 120, 230, 90 }, g_out[3] = { 20, 110, 30 };
    for (int i = 0; i < 240; i++) ht.green[i] = mix(g_in, g_out, i / 204.0f);
    static const int skin_top[3] = { 150, 140, 70 }, skin_bot[3] = { 95, 90, 40 };
    for (int y = 0; y < H; y++) ht.skin[y] = mix(skin_top, skin_bot, y / (float)H);
    ht.lid_line = rgb(40, 32, 10);
    ht.black = rgb(5, 5, 5);
    // Heavy, sleepy lids that sag and lift slowly.
    const float sag = 10.0f * sinf(t * 0.6f);
    for (int x = 0; x < W; x++) {
        const float u = (x - CX) / 233.0f;
        ht.lid[x] = (int16_t)(CY - 120.0f + sag + 70.0f * u * u);
        ht.lid_lo[x] = (int16_t)(CY + 165.0f - 50.0f * u * u);
    }
    ht.gx = CX - ms * 88; ht.gy = CY - 55;
    const int grx = 40, gry = 20;
    ht.grx2 = grx * grx; ht.gry2 = gry * gry;
    ht.glint = rgb(255, 255, 245);
}

// Shared Sharingan iris, outline and glint.
static void prep_sharingan_iris(int ms)
{
    static const int in[3] = { 255, 40, 40 }, mid[3] = { 215, 10, 20 }, out[3] = { 120, 0, 10 };
    for (int i = 0; i < 240; i++) sh.iris[i] = i < 120 ? mix(in, mid, i / 120.0f) : mix(mid, out, (i - 120) / 105.0f);
    sh.outline = rgb(30, 0, 0);
    sh.black = rgb(8, 0, 2);
    sh.white = rgb(255, 245, 245);
    sh.edge4 = 224 * 4;
    sh.glint[0] = (glint_t){ CX - ms * 72, CY - 100, 14 * 14 };
    sh.glint[1] = (glint_t){ CX - ms * 48, CY - 118, 6 * 6 };
}

// Rotation that turns slowly and every 5 s whips round one and a half turns.
static float sharingan_turn(float t, float slow)
{
    const float cycle = fmodf(t, 5.0f), dur = 1.1f;
    float spin = floorf(t / 5.0f) * 1.5f;
    if (cycle < dur) {
        const float u = cycle / dur;
        spin += 1.5f * (u * u * (3.0f - 2.0f * u));  // ease in and out
    } else {
        spin += 1.5f;
    }
    return 2.0f * PI_F * (spin + slow * t);
}

static void prep_sharingan(float t, int ms)
{
    prep_sharingan_iris(ms);
    const float rt = 128.0f, hr = 27.0f, span = 0.55f;  // ring radius, head radius, tail length (radians)
    sh.pupil4 = (int)((42.0f + 2.0f * sinf(t * 2.0f)) * 4.0f);
    sh.ring4 = (int)(rt * 4.0f);
    sh.ring_w4 = 3 * 4;
    const float th0 = sharingan_turn(t, 0.08f);
    memset(sh.head_lo, 0, sizeof(sh.head_lo));
    memset(sh.head_hi, 0, sizeof(sh.head_hi));
    memset(sh.tail_c, 0, sizeof(sh.tail_c));
    memset(sh.tail_w, 0, sizeof(sh.tail_w));
    // Only the angle steps each tomoe covers: its head, and its tail behind it.
    const float step = 2.0f * PI_F / ANG_STEPS;
    const int head_steps = (int)(asinf(hr / rt) / step) + 1, tail_steps = (int)(span / step) + 1;
    for (int k = 0; k < 3; k++) {
        const float thk = th0 + k * 2.0f * PI_F / 3.0f;
        const int centre = (int)floorf(thk / step);
        for (int j = -tail_steps; j <= head_steps; j++) {
            const int a = (centre + j) & ANG_MASK;
            const float d = (centre + j) * step - thk;  // angle from this tomoe's head
            const float s = rt * sinf(d);
            if (fabsf(s) < hr) {  // the head: a round disc on the ring
                const float c = rt * cosf(d), hh = sqrtf(hr * hr - s * s);
                sh.head_lo[a] = (uint16_t)((c - hh) * 4.0f);
                sh.head_hi[a] = (uint16_t)((c + hh) * 4.0f);
            }
            const float back = -d;  // the tail trails behind the head as it spins
            if (back > 0.0f && back < span) {
                // The tail leaves the outer side of the head, curls a little
                // further out and tapers to a point: a comma.
                const float u = back / span, v = 1.0f - u;
                sh.tail_c[a] = (uint16_t)((rt + hr * (0.35f + 0.4f * u)) * 4.0f);
                sh.tail_w[a] = (uint16_t)(hr * 0.65f * v * sqrtf(sqrtf(v)) * 4.0f);  // ~ v^1.25
            }
        }
    }
}

// Itachi-style Mangekyo: a black core with three curved pinwheel blades.
static void prep_mangekyo(float t, int ms)
{
    prep_sharingan_iris(ms);
    const float core = 52.0f, tip = 208.0f;
    sh.core4 = (int)(core * 4.0f);
    sh.rot = (int)(sharingan_turn(t, 0.05f) / (2.0f * PI_F) * ANG_STEPS);
    const float grow = 1.0f + 0.04f * sinf(t * 2.2f);
    for (int r = 0; r < 240; r++) {
        if (r < core || r >= tip * grow) {
            sh.blade_w[r] = 0;
        } else {
            const float u = (r - core) / (tip * grow - core);
            sh.blade_w[r] = (uint16_t)(ANG_STEPS / 3 * 0.5f * powf(1.0f - u, 0.9f));  // wide at the core, sharp tip
        }
        sh.twist[r] = (uint16_t)((r - core > 0 ? r - core : 0) * 1.55f);  // curved blades
    }
}
static void prep_happy(float t, int ms)
{
    static const int bg_top[3] = { 40, 22, 70 }, bg_bot[3] = { 20, 10, 40 };
    const int bob = (int)(12.0f * sinf(2.0f * PI_F * 0.8f * t));
    const int R = 118, half = 20, edge = 7;
    hp.acx = CX;
    hp.acy = CY + 55 + bob;
    hp.ro2 = (R + half) * (R + half);
    hp.ri2 = (R - half) * (R - half);
    hp.oo2 = (R + half + edge) * (R + half + edge);
    hp.oi2 = (R - half - edge) * (R - half - edge);
    hp.cap2 = half * half;
    hp.capo2 = (half + edge) * (half + edge);
    hp.lx = hp.acx - R;
    hp.rx = hp.acx + R;
    hp.bx = CX - ms * 118; hp.by = CY + 118 + bob / 2;
    const int brx = 50, bry = 24;
    hp.brx2 = brx * brx; hp.bry2 = bry * bry;
    for (int y = 0; y < H; y++) hp.bg[y] = mix(bg_top, bg_bot, y / (float)H);
    hp.cream = rgb(255, 244, 222);
    hp.outline = rgb(10, 4, 20);
    hp.blush = rgb(255, 120, 175);
    hp.spark = rgb(255, 240, 180);
    static const int pos[2][2] = { { 150, -130 }, { -165, -95 } };
    for (int i = 0; i < 2; i++) {
        const float tw = sinf(t * 2.6f + i * 2.5f);
        hp.sparks[i] = (sparkle_t){ CX + ms * pos[i][0], CY + pos[i][1], tw > 0.0f ? (int)(30.0f * tw) : 0 };
    }
}

void cartoon_prepare(int anim, float t, bool mirror)
{
    const int ms = mirror ? -1 : 1;
    anim_now = anim;
    switch (anim) {
        case A_SPARKLE: prep_sparkle(t, ms); break;
        case A_HYPNOTOAD: prep_hypnotoad(t, ms); break;
        case A_SHARINGAN: prep_sharingan(t, ms); break;
        case A_MANGEKYO: prep_mangekyo(t, ms); break;
        default: prep_happy(t, ms); break;
    }
}

// ------------------------------------------------------------------ rows

// A four-point sparkle: two thin crossed diamonds.
static inline bool in_sparkle(const sparkle_t *s, int x, int y)
{
    if (s->len <= 0) return false;
    const int dx = iabs(x - s->x), dy = iabs(y - s->y);
    if (dx > s->len || dy > s->len) return false;
    return dx * 6 + dy < s->len || dy * 6 + dx < s->len;
}

static inline uint16_t px_sparkle(int x, int y, int dy2)
{
    if (y < sp.lid_top[x]) return y >= sp.lid_top[x] - 6 ? sp.lash : BLACK_PX;
    if (y > sp.lid_bot[x]) return y <= sp.lid_bot[x] + 5 ? sp.lash : BLACK_PX;
    const int dx = x - sp.icx;
    const int d2 = dx * dx + dy2;
    if (d2 >= sp.r2) return sp.sclera[y];
    if (d2 >= sp.rin2) return sp.outline;
    int hx = x - sp.h1x, hy = y - sp.h1y;
    if (hx * hx + hy * hy < sp.h1r2) return sp.white;
    hx = x - sp.h2x; hy = y - sp.h2y;
    if (hx * hx + hy * hy < sp.h2r2) return sp.white;
    hx = x - sp.h3x; hy = y - sp.h3y;
    if (hx * hx + hy * hy < sp.h3r2) return sp.white;
    if (d2 < sp.pr2) return sp.pupil;
    if (d2 < sp.pout2) return sp.outline;
    if (d2 >= sp.ring_lo2 && d2 < sp.ring_hi2) return sp.ring[y];
    return sp.iris[y];
}

static inline uint16_t px_hypnotoad(int x, int y, int i, bool glint_row, int gy2rx2)
{
    const int lid = ht.lid[x], lo = ht.lid_lo[x];
    if (y < lid) return ht.skin[y];
    if (y < lid + 8) return ht.lid_line;
    if (y > lo) return ht.skin[y];
    if (y > lo - 6) return ht.lid_line;
    const unsigned d = dist4[i];
    if (d >= (unsigned)ht.rim4) return ht.skin[y];
    if (d >= (unsigned)ht.eye4) return ht.lid_line;
    if (glint_row) {
        const int gx = x - ht.gx;
        if (gx * gx * ht.gry2 + gy2rx2 < ht.grx2 * ht.gry2) return ht.glint;
    }
    if (d < (unsigned)ht.pupil4) return ht.black;
    if (d >= (unsigned)ht.front4) return ht.green[d >> 2];  // the rings haven't reached here yet
    const int idx = (((int)(d * ht.k) >> 8) - ht.phase) & 255;
    return d >= (unsigned)ht.dim4 ? ht.palette_dim[idx] : ht.palette[idx];
}

static inline bool in_glints(int x, int y, const glint_t *const *g, int n)
{
    for (int k = 0; k < n; k++) {
        const int dx = x - g[k]->x, dy = y - g[k]->y;
        if (dx * dx + dy * dy < g[k]->r2) return true;
    }
    return false;
}

static inline uint16_t px_sharingan(int x, int y, int i, const glint_t *const *g, int ng)
{
    const unsigned d = dist4[i];
    if (d >= (unsigned)sh.edge4) return sh.outline;
    if (ng && in_glints(x, y, g, ng)) return sh.white;
    if (d < (unsigned)sh.pupil4) return sh.black;
    const unsigned a = ang[i];
    if (d >= sh.head_lo[a] && d < sh.head_hi[a]) return sh.black;
    if ((unsigned)iabs((int)d - sh.tail_c[a]) < sh.tail_w[a]) return sh.black;
    if ((unsigned)iabs((int)d - sh.ring4) < (unsigned)sh.ring_w4) return sh.black;
    return sh.iris[d >> 2];
}

static inline uint16_t px_mangekyo(int x, int y, int i, const glint_t *const *g, int ng)
{
    const unsigned d = dist4[i];
    if (d >= (unsigned)sh.edge4) return sh.outline;
    if (ng && in_glints(x, y, g, ng)) return sh.white;
    if (d < (unsigned)sh.core4) return sh.black;
    const unsigned r = d >> 2;
    const unsigned w = sh.blade_w[r];
    if (w) {
        const int phase = ((int)ang[i] - sh.rot - sh.twist[r]) & ANG_MASK;
        if (phase % (ANG_STEPS / 3) < (int)w) return sh.black;
    }
    return sh.iris[r];
}

// Glints that touch row y.
static inline int row_glints(int y, const glint_t **out)
{
    int n = 0;
    for (int k = 0; k < 2; k++) {
        const int dy = y - sh.glint[k].y;
        if (dy * dy < sh.glint[k].r2) out[n++] = &sh.glint[k];
    }
    return n;
}

// Sparkles that touch row y (at most n), so pixels only test those.
static inline int row_sparkles(const sparkle_t *all, int n, int y, const sparkle_t **out)
{
    int k = 0;
    for (int i = 0; i < n; i++)
        if (all[i].len > 0 && iabs(y - all[i].y) <= all[i].len) out[k++] = &all[i];
    return k;
}

// Per-row facts for the Happy eye, so each pixel tests only what can touch
// its row.
typedef struct {
    int dy2;                 // squared distance of the row from the arc centre
    bool arc, caps, blush;   // the row crosses the arc / its end caps / the blush
    int by2rx2;              // blush: (y - by)^2 * rx^2
    uint16_t bg;
} happy_row_t;

static inline uint16_t px_happy(int x, int y, const happy_row_t *r, const sparkle_t *const *sk, int nsk)
{
    if (r->arc) {
        // The arc: the top half of a thick ring.
        const int dx = x - hp.acx;
        const int d2 = dx * dx + r->dy2;
        if (d2 < hp.oo2 && d2 >= hp.oi2) return (d2 < hp.ro2 && d2 >= hp.ri2) ? hp.cream : hp.outline;
    }
    if (r->caps) {  // round ends of the arc
        const int lx = x - hp.lx, rx = x - hp.rx;
        if (lx * lx + r->dy2 < hp.capo2) return lx * lx + r->dy2 < hp.cap2 ? hp.cream : hp.outline;
        if (rx * rx + r->dy2 < hp.capo2) return rx * rx + r->dy2 < hp.cap2 ? hp.cream : hp.outline;
    }
    if (r->blush) {
        const int bx = x - hp.bx;
        if (bx * bx * hp.bry2 + r->by2rx2 < hp.brx2 * hp.bry2) return hp.blush;
    }
    for (int k = 0; k < nsk; k++)
        if (in_sparkle(sk[k], x, y)) return hp.spark;
    return r->bg;
}

CARTOON_FAST void cartoon_rows(uint16_t *dst, int y0, int y1)
{
    const int anim = anim_now;
    for (int y = y0; y < y1; y++, dst += W) {
        const int x0 = row_x0[y], x1 = row_x1[y];
        memset(dst, 0, x0 * sizeof(uint16_t));
        memset(dst + x1, 0, (W - x1) * sizeof(uint16_t));
        const int row = y * W;
        switch (anim) {
            case A_SPARKLE: {
                const int dy = y - sp.icy, dy2 = dy * dy;
                for (int x = x0; x < x1; x++) dst[x] = px_sparkle(x, y, dy2);
                break;
            }
            case A_HYPNOTOAD: {
                const int gy = y - ht.gy;
                const bool glint_row = gy * gy < ht.gry2;
                const int gy2rx2 = gy * gy * ht.grx2;
                for (int x = x0; x < x1; x++) dst[x] = px_hypnotoad(x, y, row + x, glint_row, gy2rx2);
                break;
            }
            case A_SHARINGAN:
            case A_MANGEKYO: {
                const glint_t *g[2];
                const int ng = row_glints(y, g);
                if (anim == A_SHARINGAN)
                    for (int x = x0; x < x1; x++) dst[x] = px_sharingan(x, y, row + x, g, ng);
                else
                    for (int x = x0; x < x1; x++) dst[x] = px_mangekyo(x, y, row + x, g, ng);
                break;
            }
            default: {
                const sparkle_t *sk[2];
                const int nsk = row_sparkles(hp.sparks, 2, y, sk);
                const int dy = y - hp.acy, by = y - hp.by;
                const happy_row_t r = {
                    .dy2 = dy * dy,
                    .arc = dy <= 0 && dy * dy < hp.oo2,
                    .caps = dy * dy < hp.capo2,
                    .blush = by * by < hp.bry2,
                    .by2rx2 = by * by * hp.brx2,
                    .bg = hp.bg[y],
                };
                if (!r.arc && !r.caps && !r.blush && !nsk) {
                    for (int x = x0; x < x1; x++) dst[x] = r.bg;  // nothing but background
                } else {
                    for (int x = x0; x < x1; x++) dst[x] = px_happy(x, y, &r, sk, nsk);
                }
                break;
            }
        }
    }
}
