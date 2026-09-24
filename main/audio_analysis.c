#include "audio_analysis.h"

#include <math.h>
#include <string.h>

#define BIN_HZ        ((float)AUDIO_SAMPLE_RATE / AUDIO_FRAME)
#define BASS_LO_HZ    40.0f
#define BASS_HI_HZ    160.0f
#define HIGH_LO_HZ    2000.0f
#define HIGH_HI_HZ    8000.0f
#define BAND_AVG_S    4.0f
#define MIN_FLUX      0.05f

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static float smoothstep(float e0, float e1, float x)
{
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// In-place iterative radix-2 FFT, n a power of two.
static void fft(float *re, float *im, int n)
{
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            float t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -2.0f * (float)M_PI / len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                int p = i + k, q = p + len / 2;
                float tr = re[q] * cr - im[q] * ci;
                float ti = re[q] * ci + im[q] * cr;
                re[q] = re[p] - tr; im[q] = im[p] - ti;
                re[p] += tr;        im[p] += ti;
                float ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
}

void audio_analysis_init(audio_analysis_t *a, float sample_rate)
{
    memset(a, 0, sizeof(*a));
    a->frame_s = AUDIO_FRAME / sample_rate;
    for (int i = 0; i < AUDIO_FRAME; i++) {
        a->window[i] = 0.5f - 0.5f * cosf(2.0f * (float)M_PI * i / (AUDIO_FRAME - 1));
    }
    a->agc_floor_db = -70.0f;
    a->agc_peak_db = -40.0f;
    a->bass_avg = a->high_avg = 1e-9f;
    a->since_beat_s = 10.0f;
    a->out.level_db = -120.0f;
    a->out.warmth = 0.5f;
}

static float median(const float *v, int n)
{
    float s[BEAT_HISTORY];
    memcpy(s, v, n * sizeof(float));
    for (int i = 1; i < n; i++) {
        float x = s[i];
        int j = i - 1;
        for (; j >= 0 && s[j] > x; j--) s[j + 1] = s[j];
        s[j + 1] = x;
    }
    return (n & 1) ? s[n / 2] : 0.5f * (s[n / 2 - 1] + s[n / 2]);
}

void audio_analysis_process(audio_analysis_t *a, const float *samples)
{
    audio_features_t *o = &a->out;
    const float dt = a->frame_s;

    // Level
    float sq = 0.0f;
    for (int i = 0; i < AUDIO_FRAME; i++) sq += samples[i] * samples[i];
    float level_db = 10.0f * log10f(sq / AUDIO_FRAME + 1e-12f);
    o->level_db = level_db;

    // Loudness via automatic gain: map level between a slow floor and a slow peak.
    if (level_db < a->agc_floor_db) a->agc_floor_db += 0.3f * (level_db - a->agc_floor_db);
    else a->agc_floor_db += AGC_FLOOR_RISE_DB_S * dt;
    if (level_db > a->agc_peak_db) a->agc_peak_db = level_db;
    else a->agc_peak_db -= AGC_PEAK_FALL_DB_S * dt;
    if (a->agc_floor_db > a->agc_peak_db - AGC_MIN_RANGE_DB) a->agc_floor_db = a->agc_peak_db - AGC_MIN_RANGE_DB;
    float range = fmaxf(a->agc_peak_db - a->agc_floor_db, AGC_MIN_RANGE_DB);
    float gate = smoothstep(SILENCE_DB, SILENCE_DB + 6.0f, level_db);
    o->loudness = clampf((level_db - a->agc_floor_db) / range, 0.0f, 1.0f) * gate;

    // Spectrum
    for (int i = 0; i < AUDIO_FRAME; i++) {
        a->re[i] = samples[i] * a->window[i];
        a->im[i] = 0.0f;
    }
    fft(a->re, a->im, AUDIO_FRAME);

    const float bin_hz = BIN_HZ;
    const int bass_lo = (int)ceilf(BASS_LO_HZ / bin_hz), bass_hi = (int)(BASS_HI_HZ / bin_hz);
    const int high_lo = (int)ceilf(HIGH_LO_HZ / bin_hz);
    const int high_hi = (int)fminf(HIGH_HI_HZ / bin_hz, AUDIO_BINS - 1);
    const float norm = 2.0f / AUDIO_FRAME;

    float bass_pow = 0.0f, high_pow = 0.0f, flux = 0.0f;
    for (int k = bass_lo; k <= bass_hi; k++) {
        float mag = sqrtf(a->re[k] * a->re[k] + a->im[k] * a->im[k]) * norm;
        bass_pow += mag * mag;
        float lm = log10f(1.0f + 1000.0f * mag);
        float d = lm - a->prev_bass[k];
        if (d > 0.0f) flux += d;
        a->prev_bass[k] = lm;
    }
    for (int k = high_lo; k <= high_hi; k++) {
        float mag = sqrtf(a->re[k] * a->re[k] + a->im[k] * a->im[k]) * norm;
        high_pow += mag * mag;
    }

    // Frequency balance relative to each band's own recent average, so it
    // self-calibrates to the mic and the venue.
    float alpha = dt / BAND_AVG_S;
    a->bass_avg += alpha * (bass_pow - a->bass_avg);
    a->high_avg += alpha * (high_pow - a->high_avg);
    float rb = bass_pow / (a->bass_avg + 1e-12f);
    float rh = high_pow / (a->high_avg + 1e-12f);
    float warmth = (rb + rh) > 1e-6f ? rb / (rb + rh) : 0.5f;
    o->warmth += 0.25f * (gate * warmth + (1.0f - gate) * 0.5f - o->warmth);

    // Beat: bass spectral flux above an adaptive threshold.
    a->since_beat_s += dt;
    if (a->flux_filled >= FLUX_HISTORY / 2) {
        float mean = 0.0f, var = 0.0f;
        for (int i = 0; i < a->flux_filled; i++) mean += a->flux_hist[i];
        mean /= a->flux_filled;
        for (int i = 0; i < a->flux_filled; i++) {
            float d = a->flux_hist[i] - mean;
            var += d * d;
        }
        float thresh = mean + BEAT_THRESHOLD_K * sqrtf(var / a->flux_filled);
        if (flux > thresh && flux > MIN_FLUX && level_db > SILENCE_DB &&
            a->since_beat_s >= BEAT_MIN_INTERVAL_S) {
            if (a->since_beat_s < 1.5f) {
                a->intervals[a->interval_pos] = a->since_beat_s;
                a->interval_pos = (a->interval_pos + 1) % BEAT_HISTORY;
                if (a->interval_filled < BEAT_HISTORY) a->interval_filled++;
            }
            a->since_beat_s = 0.0f;
            o->beat_count++;
        }
    }
    a->flux_hist[a->flux_pos] = flux;
    a->flux_pos = (a->flux_pos + 1) % FLUX_HISTORY;
    if (a->flux_filled < FLUX_HISTORY) a->flux_filled++;

    if (a->since_beat_s > 3.0f) a->interval_filled = 0;  // music stopped; forget tempo
    o->beat_period_s = a->interval_filled >= 3 ? median(a->intervals, a->interval_filled) : 0.0f;
}
