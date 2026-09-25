#include "nibbles_link.h"

#include <math.h>
#include <string.h>

// Beat intervals outside this range are ignored (double hits, long gaps).
#define MIN_INTERVAL_MS  250
#define MAX_INTERVAL_MS  1500
#define TOLERANCE        0.08f   // intervals within 8% of the median count as steady
#define MIN_CONFIDENT    4       // intervals needed before a tempo is reported
#define AVG_TAU_S        1.5f
#define STALE_MS         3000    // no peaks for this long: the tempo is forgotten

void nl_ar_init(nl_ar_state_t *s)
{
    memset(s, 0, sizeof(*s));
    s->avg_db = -60.0f;
}

static float fold_period(float p)
{
    // Same range as the eyes' own tempo (100..200 bpm).
    while (p > 0.6f) p *= 0.5f;
    while (p < 0.3f) p *= 2.0f;
    return p;
}

static void sort(float *v, int n)
{
    for (int i = 1; i < n; i++) {
        const float x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
        v[j + 1] = x;
    }
}

void nl_ar_update(nl_ar_state_t *s, float volume, const uint8_t fft[16], bool peak, uint32_t now_ms, nl_audio_t *out)
{
    const float dt = s->last_ms ? (now_ms - s->last_ms) / 1000.0f : 0.0f;
    s->last_ms = now_ms;

    // AudioReactive's smoothed volume is 0..255 after its own gain control.
    const float level_db = 20.0f * log10f(fmaxf(volume, 0.5f) / 255.0f);
    s->avg_db += (level_db - s->avg_db) * (1.0f - expf(-dt / AVG_TAU_S));

    // Beats: AudioReactive flags peaks, often several per beat (seen live:
    // about 9 a second). A peak within MIN_INTERVAL_MS of the last counted
    // beat is part of the same beat, so at most 240 beats a minute count.
    if (peak && !s->peak_was) s->peak_count++;  // every peak, for peak-reactive presets
    if (peak && !s->peak_was && (!s->last_peak_ms || now_ms - s->last_peak_ms >= MIN_INTERVAL_MS)) {
        s->beat_count++;
        if (s->last_peak_ms) {
            const uint32_t iv = now_ms - s->last_peak_ms;
            if (iv <= MAX_INTERVAL_MS) {
                s->intervals[s->pos] = iv / 1000.0f;
                s->pos = (s->pos + 1) % NL_AR_INTERVALS;
                if (s->filled < NL_AR_INTERVALS) s->filled++;
            }
        }
        s->last_peak_ms = now_ms;
    }
    s->peak_was = peak;
    if (s->last_peak_ms && now_ms - s->last_peak_ms > STALE_MS) s->filled = 0;

    float period = 0.0f, confidence = 0.0f;
    if (s->filled >= MIN_CONFIDENT) {
        float v[NL_AR_INTERVALS];
        memcpy(v, s->intervals, sizeof(v));
        sort(v, s->filled);
        const float median = v[s->filled / 2];
        int steady = 0;
        for (int i = 0; i < s->filled; i++) steady += fabsf(v[i] - median) <= TOLERANCE * median;
        confidence = (float)steady / s->filled;
        if (confidence >= 0.75f) period = fold_period(median);
    }

    // Warmth: bass (bands 0-3) against treble (bands 8-15).
    float bass = 0.0f, treble = 0.0f;
    for (int i = 0; i < 4; i++) bass += fft[i];
    for (int i = 8; i < 16; i++) treble += fft[i];
    bass /= 4.0f;
    treble /= 8.0f;
    const float warmth = bass + treble > 1.0f ? bass / (bass + treble) : 0.5f;

    *out = (nl_audio_t){
        .level_db = level_db,
        .avg_db = s->avg_db,
        .noise_floor_db = -60.0f,  // AudioReactive handles its own noise gate
        .gain_db = 0.0f,
        .loudness = fminf(fmaxf(volume / 255.0f, 0.0f), 1.0f),
        .warmth = warmth,
        .beat_period_s = period,
        .beat_confidence = confidence,
        .beat_count = s->beat_count,
        .peak_count = s->peak_count,
    };
}
