// Music analysis: loudness, frequency balance and beat detection.
// Pure C with no ESP-IDF dependencies so it can be tested on the host.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "config.h"

#define AUDIO_BINS        (AUDIO_FRAME / 2)
#define FLUX_HISTORY      48   // ~1.5 s of frames
#define BEAT_HISTORY      8

typedef struct {
    float level_db;        // RMS level in dBFS with the mic gain removed
    float noise_floor_db;  // background level, same scale as level_db
    bool sound;            // this frame is clearly above the background
    float gain_db;         // mic gain the analysis wants applied
    float loudness;        // 0..1, automatic-gain normalised
    float warmth;          // 0 = treble-heavy, 0.5 = balanced, 1 = bass-heavy
    uint32_t beat_count;   // increments on every detected beat
    float beat_period_s;   // median time between recent beats, 0 if unknown
} audio_features_t;

typedef struct {
    float frame_s;
    float window[AUDIO_FRAME];
    float re[AUDIO_FRAME], im[AUDIO_FRAME];
    float prev_bass[AUDIO_BINS];
    float flux_hist[FLUX_HISTORY];
    int flux_pos, flux_filled;
    float agc_floor_db, agc_peak_db;
    float bass_avg, high_avg;
    float since_beat_s;
    float intervals[BEAT_HISTORY];
    int interval_pos, interval_filled;
    float noise_floor_db;
    bool have_floor;
    float floor_age_s;
    float applied_gain_db;     // gain in effect for the samples being analysed
    float peak_env_db;         // raw peak envelope, for the gain control
    float raise_timer_s, since_change_s;
    int settle;
    bool rebase_flux;
    audio_features_t out;
} audio_analysis_t;

void audio_analysis_init(audio_analysis_t *a, float sample_rate, float start_gain_db);

// Feed one frame of AUDIO_FRAME raw mono samples in -1..1, as captured with the
// gain from the previous a->out.gain_db. Updates a->out; if out.gain_db changed,
// the caller should apply it to the mic.
void audio_analysis_process(audio_analysis_t *a, const float *samples);
