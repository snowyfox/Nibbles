#include "audio_analysis.h"

#include <math.h>
#include <string.h>

#define BIN_HZ        ((float)AUDIO_SAMPLE_RATE / AUDIO_FRAME)
#define BASS_LO_HZ    40.0f
#define BASS_HI_HZ    160.0f
#define HIGH_LO_HZ    2000.0f
#define HIGH_HI_HZ    8000.0f
#define BAND_AVG_S    4.0f

// Beats are onsets measured across the spectrum: small mics hear little of
// the kick drum's bass, but the rhythm shows clearly in the mids and highs.
static const float onset_band_hz[ONSET_BANDS + 1] = { 40, 160, 400, 1000, 3000, 8000 };
static const float onset_weight[ONSET_BANDS] = ONSET_WEIGHTS;

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

void audio_analysis_init(audio_analysis_t *a, float sample_rate, float start_gain_db)
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
    a->applied_gain_db = a->out.gain_db = start_gain_db;
    a->peak_env_db = AGC_TARGET_PEAK_DB;
    a->settle = AGC_SETTLE_FRAMES;
}

// Decide the mic gain from the raw (gained) peak level.
static void update_gain(audio_analysis_t *a, float peak_db, float dt)
{
    a->since_change_s += dt;
    a->peak_env_db = fmaxf(a->peak_env_db - AGC_PEAK_RELEASE_DB_S * dt, peak_db);
    a->raise_timer_s = a->peak_env_db < AGC_TARGET_PEAK_DB - 6.0f ? a->raise_timer_s + dt : 0.0f;

    float gain = a->out.gain_db;
    if (peak_db > AGC_CLIP_DB && a->since_change_s >= 0.25f) {
        gain = fmaxf(MIC_GAIN_MIN_DB, gain - 2.0f * MIC_GAIN_STEP_DB);
    } else if (a->raise_timer_s >= AGC_RAISE_HOLD_S && a->since_change_s >= AGC_RAISE_HOLD_S) {
        gain = fminf(MIC_GAIN_MAX_DB, gain + MIC_GAIN_STEP_DB);
    }
    if (gain != a->out.gain_db) {
        a->peak_env_db += gain - a->out.gain_db;  // expect peaks to move with the gain
        a->out.gain_db = gain;
        a->raise_timer_s = a->since_change_s = 0.0f;
        a->settle = AGC_SETTLE_FRAMES;
    }
}

// How periodic recent onsets are: the strongest normalised autocorrelation
// peak at a musical tempo. Music is strongly periodic; noise, a held chord
// and talking are not.
static float rhythm_strength(const audio_analysis_t *a)
{
    const int n = ONSET_HISTORY;
    float x[ONSET_HISTORY];
    float mean = 0.0f, r0 = 0.0f;
    for (int i = 0; i < n; i++) mean += a->onset[i];
    mean /= n;
    for (int i = 0; i < n; i++) {
        x[i] = a->onset[(a->onset_pos + i) % n] - mean;
        r0 += x[i] * x[i];
    }
    if (r0 < 1e-12f) return 0.0f;
    const float fps = 1.0f / a->frame_s;
    const int lo = (int)(fps * 60.0f / BEAT_MAX_BPM), hi = (int)(fps * 60.0f / BEAT_MIN_BPM) + 1;
    float best = 0.0f;
    for (int lag = lo; lag <= hi; lag++) {
        float s = 0.0f;
        for (int i = 0; i + lag < n; i++) s += x[i] * x[i + lag];
        best = fmaxf(best, s / r0 * (float)n / (n - lag));
    }
    return best;
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

    // Raw peak drives the mic gain control.
    float peak = 0.0f;
    for (int i = 0; i < AUDIO_FRAME; i++) peak = fmaxf(peak, fabsf(samples[i]));
    const float peak_db = 20.0f * log10f(peak + 1e-9f);

    // Right after a gain change the buffered samples may have either gain, so
    // skip a few frames, keeping the beat detector's reference current.
    if (a->settle > 0) {
        a->settle--;
        a->applied_gain_db = o->gain_db;
        a->rebase_flux = true;
        return;
    }
    update_gain(a, peak_db, dt);

    // Level with the mic gain removed.
    const float inv_gain = powf(10.0f, -a->applied_gain_db / 20.0f);
    float sq = 0.0f;
    for (int i = 0; i < AUDIO_FRAME; i++) sq += samples[i] * samples[i];
    const float level_db = 10.0f * log10f(sq / AUDIO_FRAME + 1e-12f) - a->applied_gain_db;
    o->level_db = level_db;
    const float power = powf(10.0f, level_db / 10.0f);
    a->avg_power = a->have_floor ? a->avg_power + dt / QUIET_AVG_S * (power - a->avg_power) : power;
    o->avg_db = 10.0f * log10f(a->avg_power + 1e-15f);

    // Background noise floor: follows dips quickly, creeps up slowly, so it
    // settles at the quiet moments between beats and at steady crowd noise.
    // Start at the first reading; the QUIET_DB cap below stops music heard at
    // boot being taken as background. Rise faster at first to learn the room.
    if (!a->have_floor) {
        a->noise_floor_db = level_db;
        a->have_floor = true;
    } else if (level_db < a->noise_floor_db) {
        a->noise_floor_db += 0.1f * (level_db - a->noise_floor_db);
    } else {
        a->floor_age_s += dt;
        a->noise_floor_db += (a->floor_age_s < NOISE_FLOOR_LEARN_S ? NOISE_FLOOR_LEARN_RISE_DB_S
                                                                   : NOISE_FLOOR_RISE_DB_S) * dt;
    }
    if (a->noise_floor_db > QUIET_DB) a->noise_floor_db = QUIET_DB;
    o->noise_floor_db = a->noise_floor_db;

    // Loudness via automatic gain: map level between a slow floor and a slow peak.
    if (level_db < a->agc_floor_db) a->agc_floor_db += 0.3f * (level_db - a->agc_floor_db);
    else a->agc_floor_db += AGC_FLOOR_RISE_DB_S * dt;
    if (level_db > a->agc_peak_db) a->agc_peak_db = level_db;
    else a->agc_peak_db -= AGC_PEAK_FALL_DB_S * dt;
    if (a->agc_floor_db > a->agc_peak_db - AGC_MIN_RANGE_DB) a->agc_floor_db = a->agc_peak_db - AGC_MIN_RANGE_DB;
    float range = fmaxf(a->agc_peak_db - a->agc_floor_db, AGC_MIN_RANGE_DB);
    float gate = smoothstep(a->noise_floor_db + 3.0f, a->noise_floor_db + 9.0f, level_db);
    o->loudness = clampf((level_db - a->agc_floor_db) / range, 0.0f, 1.0f) * gate;

    // Spectrum
    for (int i = 0; i < AUDIO_FRAME; i++) {
        a->re[i] = samples[i] * inv_gain * a->window[i];
        a->im[i] = 0.0f;
    }
    fft(a->re, a->im, AUDIO_FRAME);

    const float bin_hz = BIN_HZ;
    const int bass_lo = (int)ceilf(BASS_LO_HZ / bin_hz), bass_hi = (int)(BASS_HI_HZ / bin_hz);
    const int high_lo = (int)ceilf(HIGH_LO_HZ / bin_hz);
    const int high_hi = (int)fminf(HIGH_HI_HZ / bin_hz, AUDIO_BINS - 1);
    const float norm = 2.0f / AUDIO_FRAME;

    // Magnitudes, stored over re[].
    for (int k = 1; k < AUDIO_BINS; k++) a->re[k] = sqrtf(a->re[k] * a->re[k] + a->im[k] * a->im[k]) * norm;
    const float *mag = a->re;

    float bass_pow = 0.0f, high_pow = 0.0f;
    for (int k = bass_lo; k <= bass_hi; k++) bass_pow += mag[k] * mag[k];
    for (int k = high_lo; k <= high_hi; k++) high_pow += mag[k] * mag[k];

    // Onset strength: in each band, positive jumps in log magnitude relative
    // to the band's own recent level (so it works at any volume), averaged per
    // bin, then weighted across bands.
    float flux = 0.0f, wsum = 0.0f;
    for (int b = 0; b < ONSET_BANDS; b++) {
        const int lo = (int)ceilf(onset_band_hz[b] / bin_hz);
        const int hi = (int)fminf(onset_band_hz[b + 1] / bin_hz, AUDIO_BINS);
        float pow_b = 0.0f;
        for (int k = lo; k < hi; k++) pow_b += mag[k] * mag[k];
        if (a->band_avg[b] <= 0.0f) a->band_avg[b] = pow_b;
        const float ref = sqrtf(a->band_avg[b] / (hi - lo)) + 1e-9f;
        float fb = 0.0f;
        for (int k = lo; k < hi; k++) {
            const float lm = log10f(1.0f + mag[k] / ref);
            const float d = lm - a->prev_logmag[k];
            if (d > 0.0f) fb += d;
            a->prev_logmag[k] = lm;
        }
        a->band_avg[b] += dt / BAND_AVG_S * (pow_b - a->band_avg[b]);
        flux += onset_weight[b] * fb / (hi - lo);
        wsum += onset_weight[b];
    }
    flux /= wsum;

    // Frequency balance relative to each band's own recent average, so it
    // self-calibrates to the mic and the venue.
    float alpha = dt / BAND_AVG_S;
    a->bass_avg += alpha * (bass_pow - a->bass_avg);
    a->high_avg += alpha * (high_pow - a->high_avg);
    float rb = bass_pow / (a->bass_avg + 1e-12f);
    float rh = high_pow / (a->high_avg + 1e-12f);
    float warmth = (rb + rh) > 1e-6f ? rb / (rb + rh) : 0.5f;
    o->warmth += 0.25f * (gate * warmth + (1.0f - gate) * 0.5f - o->warmth);

    // Beats: onsets clearly above the recent level, but only while the music
    // is rhythmic. Checking rhythm (how periodic the last ~4 s of onsets are)
    // lets the onset threshold be sensitive enough for faint music without
    // noise, a held chord or talking producing beats.
    const bool rebase = a->rebase_flux;  // first frame after a gap: flux is meaningless
    a->rebase_flux = false;
    a->since_beat_s += dt;
    if (!rebase) {
        a->onset_mean += 0.05f * (flux - a->onset_mean);
        a->onset[a->onset_pos] = fmaxf(0.0f, flux - a->onset_mean);
        a->onset_pos = (a->onset_pos + 1) % ONSET_HISTORY;
        if (a->onset_filled < ONSET_HISTORY) a->onset_filled++;
    }
    if (a->onset_filled == ONSET_HISTORY && --a->eval_countdown <= 0) {
        a->eval_countdown = BEAT_EVAL_FRAMES;
        o->beat_confidence += 0.5f * (rhythm_strength(a) - o->beat_confidence);
    }

    if (!rebase && a->flux_filled >= FLUX_HISTORY / 2) {
        float mean = 0.0f, var = 0.0f;
        for (int i = 0; i < a->flux_filled; i++) mean += a->flux_hist[i];
        mean /= a->flux_filled;
        for (int i = 0; i < a->flux_filled; i++) {
            float d = a->flux_hist[i] - mean;
            var += d * d;
        }
        const float thresh = mean + BEAT_THRESHOLD_K * sqrtf(var / a->flux_filled);
        // Peaks: every rise above the threshold while there is real sound,
        // without the rhythm and minimum-interval gates that beats have.
        const bool above = flux > thresh && level_db > a->noise_floor_db + 3.0f;
        if (above && !a->above_thresh) o->peak_count++;
        a->above_thresh = above;
        if (flux > thresh && o->beat_confidence >= BEAT_MIN_CONFIDENCE &&
            level_db > a->noise_floor_db + 3.0f && a->since_beat_s >= BEAT_MIN_INTERVAL_S) {
            if (a->since_beat_s < 1.5f) {
                // Fold into the tempo range so beats on eighth notes and on
                // quarter notes agree on the same musical beat.
                float iv = a->since_beat_s;
                while (iv < 60.0f / TEMPO_FOLD_MAX_BPM) iv *= 2.0f;
                while (iv > 60.0f / TEMPO_FOLD_MIN_BPM) iv *= 0.5f;
                a->intervals[a->interval_pos] = iv;
                a->interval_pos = (a->interval_pos + 1) % BEAT_HISTORY;
                if (a->interval_filled < BEAT_HISTORY) a->interval_filled++;
            }
            a->since_beat_s = 0.0f;
            o->beat_count++;
        }
    }
    if (!rebase) {
        a->flux_hist[a->flux_pos] = flux;
        a->flux_pos = (a->flux_pos + 1) % FLUX_HISTORY;
        if (a->flux_filled < FLUX_HISTORY) a->flux_filled++;
    }

    if (a->since_beat_s > 3.0f) a->interval_filled = 0;  // music stopped; forget tempo
    o->beat_period_s = a->interval_filled >= 3 ? median(a->intervals, a->interval_filled) : 0.0f;
}
