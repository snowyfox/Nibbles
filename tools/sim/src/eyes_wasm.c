// Both eyes in the browser: the eye firmware's own analysis, behaviour and
// drawing code (firmware/eyes/main), driven the way main.c drives it. The
// starboard eye leads (presets, blinks, bumps); the port eye follows its
// shared state, as over the eye cable.
//
// Audio comes either from the port eye's own analysis (samples at
// AUDIO_SAMPLE_RATE) or from WLED's AudioReactive analysis converted by
// nl_ar_update, as the real eyes do with EYES_AUDIO_FROM_WLED.
#include <emscripten/emscripten.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "audio_analysis.h"
#include "config.h"
#include "eye.h"
#include "nibbles_link.h"
#include "presets.h"
#include "render_core.h"

enum { SRC_WLED = 0, SRC_PORT_MIC = 1 };

static audio_analysis_t ears;          // the port eye's mic analysis
static float mic_block[AUDIO_FRAME];
static nl_ar_state_t ar;
static nl_audio_t wled_audio;
static uint32_t wled_ms;
static int source = SRC_WLED;

static eye_t lead, port;
static int preset_index, next_preset, rendered, bump_preset = -1;
static bool auto_cycle = true;
static float swap_in_s = PRESET_CYCLE_S;
static nl_react_t react = NL_REACT_PRESET;
static float now_s;

static uint32_t rgba[2][DISP_W * DISP_H];
static uint16_t rows[DISP_W * DISP_H];
static int brightness = 100;

// What the page reads back after each frame.
typedef struct {
    int preset, rendered, preset_count, state, auto_cycle, peaks_now, source;
    float tempo_bpm, level_db, loudness, hype, gain_db;
    uint32_t beat_count, peak_count;
} eyes_status_t;
static eyes_status_t status;

EMSCRIPTEN_KEEPALIVE int eyes_init(uint32_t seed)
{
    if (!render_core_init(malloc)) return 0;
    audio_analysis_init(&ears, AUDIO_SAMPLE_RATE, MIC_GAIN_START_DB);
    nl_ar_init(&ar);
    eye_init(&lead, seed);
    eye_init(&port, seed * 2654435761u + 1);
    render_core_set_preset(0);
    return 1;
}

EMSCRIPTEN_KEEPALIVE float *eyes_mic_buffer(void) { return mic_block; }
EMSCRIPTEN_KEEPALIVE int eyes_mic_frame(void) { return AUDIO_FRAME; }
EMSCRIPTEN_KEEPALIVE int eyes_mic_rate(void) { return AUDIO_SAMPLE_RATE; }

// One frame of mic samples in -1..1 at 0 dB; the analysis's own gain control
// is applied here, like the codec's input gain on the eye.
EMSCRIPTEN_KEEPALIVE void eyes_mic_process(void)
{
    const float g = powf(10.0f, ears.out.gain_db / 20.0f);
    for (int i = 0; i < AUDIO_FRAME; i++) mic_block[i] = fmaxf(-1.0f, fminf(1.0f, mic_block[i] * g));
    audio_analysis_process(&ears, mic_block);
}

// WLED's AudioReactive output for one block (what the usermod reads from um_data).
EMSCRIPTEN_KEEPALIVE void eyes_wled_audio(float volume, const uint8_t *fft, int peak)
{
    wled_ms += 23;  // one 512-sample AudioReactive block at 22050 Hz
    nl_ar_update(&ar, volume, fft, peak != 0, wled_ms, &wled_audio);
}

EMSCRIPTEN_KEEPALIVE void eyes_source(int s) { source = s; }
EMSCRIPTEN_KEEPALIVE void eyes_brightness(int pct) { brightness = pct; }

// Radio commands, as from the base station.
EMSCRIPTEN_KEEPALIVE void eyes_command(int op, int arg)
{
    if (op == NL_OP_AUTO_CYCLE) { auto_cycle = arg != 0; swap_in_s = PRESET_CYCLE_S; return; }
    if (op == NL_OP_REACTIVITY) { react = (nl_react_t)arg; return; }
    const int base = lead.swap_pending ? next_preset : preset_index;
    if (op == NL_OP_PRESET_SET) next_preset = ((arg % preset_count) + preset_count) % preset_count;
    else if (op == NL_OP_PRESET_NEXT) next_preset = (base + 1) % preset_count;
    else if (op == NL_OP_PRESET_PREV) next_preset = (base + preset_count - 1) % preset_count;
    else return;
    eye_request_swap(&lead);
    auto_cycle = false;  // a preset picked by hand stays until the next pick
}

// Bumps: action NL_BUMP_* while held, 0 on release. Preset bumps take a
// 1-based preset number, as on the radio.
EMSCRIPTEN_KEEPALIVE void eyes_bump(int action, int arg)
{
    eye_set_bump(&lead, (uint8_t)action);
    bump_preset = action == NL_BUMP_PRESET && arg >= 1 && arg <= preset_count ? arg - 1 : -1;
}

static void from_nl(const nl_audio_t *r, audio_features_t *out)
{
    memset(out, 0, sizeof(*out));
    out->level_db = r->level_db;
    out->avg_db = r->avg_db;
    out->noise_floor_db = r->noise_floor_db;
    out->gain_db = r->gain_db;
    out->loudness = r->loudness;
    out->warmth = r->warmth;
    out->beat_period_s = r->beat_period_s;
    out->beat_confidence = r->beat_confidence;
    out->beat_count = r->beat_count;
    out->peak_count = r->peak_count;
}

static void to_rgba(uint32_t *dst)
{
    const int k = brightness * 256 / 100;
    for (int i = 0; i < DISP_W * DISP_H; i++) {
        const uint16_t v = (uint16_t)((rows[i] >> 8) | (rows[i] << 8));  // the panel's byte order
        const uint32_t r = ((v >> 11) & 31) * 255 / 31, g = ((v >> 5) & 63) * 255 / 63, b = (v & 31) * 255 / 31;
        dst[i] = 0xff000000u | ((b * k >> 8) << 16) | ((g * k >> 8) << 8) | (r * k >> 8);
    }
}

// One eye frame: behaviour for both eyes, then both pictures (RGBA).
EMSCRIPTEN_KEEPALIVE void eyes_frame(float dt)
{
    if (dt > 0.1f) dt = 0.1f;
    now_s += dt;
    if (auto_cycle && (swap_in_s -= dt) <= 0) {
        swap_in_s += PRESET_CYCLE_S;
        next_preset = (preset_index + 1) % preset_count;
        eye_request_swap(&lead);
    }
    audio_features_t mic = ears.out, a;
    if (source == SRC_WLED) from_nl(&wled_audio, &a);
    else a = mic;
    const motion_features_t still = { 0 };  // the simulator doesn't move

    // The leader (starboard), as in main.c when not following.
    eye_update(&lead, &a, &still, dt);
    const int wanted = bump_preset >= 0 ? bump_preset : next_preset;
    if (wanted != rendered && !lead.swap_pending && !lead.p.swap_now) eye_request_swap(&lead);
    if (lead.p.swap_now) {
        preset_index = next_preset;
        const int show = bump_preset >= 0 ? bump_preset : preset_index;
        if (show != rendered) {
            rendered = show;
            render_core_set_preset(rendered);
        }
    }
    const bool peaks = react == NL_REACT_PRESET ? presets[rendered].peak_beats : react == NL_REACT_PEAKS;
    eye_set_peak_beats(&lead, peaks);
    nl_eye_state_t shared;
    eye_export_shared(&lead, rendered, NL_SIDE_STARBOARD, &shared);

    // The port eye runs on its own mic and takes the leader's shared state.
    eye_update(&port, &mic, &still, dt);
    eye_apply_shared(&port, &shared, NL_SIDE_PORT, &still);

    render_core_set_mirror(false);  // the port eye's spirals are mirrored, as on the eyes
    render_core_prepare(&lead.p);
    render_core_rows(rows, 0, DISP_H);
    to_rgba(rgba[0]);
    render_core_set_mirror(true);
    render_core_prepare(&port.p);
    render_core_rows(rows, 0, DISP_H);
    to_rgba(rgba[1]);

    status = (eyes_status_t){
        .preset = preset_index, .rendered = rendered, .preset_count = preset_count,
        .state = lead.state, .auto_cycle = auto_cycle, .peaks_now = lead.peak_beats, .source = source,
        .tempo_bpm = lead.p.tempo_bpm, .level_db = a.level_db, .loudness = a.loudness,
        .hype = lead.p.hype, .gain_db = ears.out.gain_db,
        .beat_count = a.beat_count, .peak_count = a.peak_count,
    };
}

EMSCRIPTEN_KEEPALIVE uint32_t *eyes_image(int i) { return rgba[i & 1]; }
EMSCRIPTEN_KEEPALIVE int eyes_size(void) { return DISP_W; }
EMSCRIPTEN_KEEPALIVE eyes_status_t *eyes_status(void) { return &status; }
EMSCRIPTEN_KEEPALIVE int eyes_preset_count(void) { return preset_count; }
EMSCRIPTEN_KEEPALIVE const char *eyes_preset_name(int i) { return presets[i % preset_count].name; }
EMSCRIPTEN_KEEPALIVE const char *eyes_state_name(int s) { return eye_state_name((eye_state_t)s); }
