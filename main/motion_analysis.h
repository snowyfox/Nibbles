// Motion analysis: gravity estimate, springy "look" offset and dance detection.
// Pure C with no ESP-IDF dependencies so it can be tested on the host.
#pragma once

#include "config.h"

#define DANCE_RATE_HZ   50
#define DANCE_SAMPLES   (DANCE_WINDOW_S * DANCE_RATE_HZ)

typedef struct {
    float look_x, look_y;   // pupil offset in -1..1, screen coords (x right, y down)
    float jolt_g;           // recent peak linear acceleration, decays quickly
    float energy_g;         // rms linear acceleration over the dance window
    float dance_score;      // 0..1
    float dance_period_s;   // period of the detected rhythm, 0 if none
} motion_features_t;

typedef struct {
    float dt;
    float grav[3];
    int have_grav;
    float pos[2], vel[2];
    // decimation to DANCE_RATE_HZ
    int decim, decim_n;
    float acc_v, acc_h;
    // ring buffers of vertical and horizontal linear acceleration
    float buf_v[DANCE_SAMPLES], buf_h[DANCE_SAMPLES];
    int buf_pos, buf_filled, since_eval;
    motion_features_t out;
} motion_analysis_t;

void motion_analysis_init(motion_analysis_t *m, float rate_hz);

// acc in g, gyro in deg/s, both in the IMU's own axes.
void motion_analysis_update(motion_analysis_t *m, const float acc[3], const float gyro[3]);
