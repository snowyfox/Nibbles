// Motion analysis: gravity estimate, springy "look" offset and dance detection.
// Pure C with no ESP-IDF dependencies so it can be tested on the host.
#pragma once

#include "config.h"

#define DANCE_RATE_HZ   50
#define DANCE_SAMPLES   (DANCE_WINDOW_S * DANCE_RATE_HZ)

typedef struct {
    float look_x, look_y;   // pupil offset in -1..1, screen coords (x right, y down)
    float twist_dps;        // rotation about the vertical; + is counter-clockwise seen from above
    float twist_dominance;  // 0..1, how much of the current motion is twist
    float jolt_g;           // recent peak linear acceleration, decays quickly
    float energy_g;         // rms linear acceleration over the dance window
    float dance_score;      // 0..1
    float dance_period_s;   // period of the detected rhythm, 0 if none
    float activity;         // 0..1 how vigorously the pole is moving (any kind of motion)
} motion_features_t;

typedef struct {
    float dt;
    float grav[3];
    int have_grav;
    float pos[2], vel[2];
    float twist_avg, other_avg;  // smoothed motion magnitudes, in look units
    float prev_twist, twist_accel;  // deg/s and smoothed deg/s^2
    float gyro_ms, lin_ms;          // ~1 s mean squares of rotation and linear acceleration
    float mount_x_m, mount_out_m;   // eye offset from the twist axis: along screen +x, and outward
    float twist_snap[2];            // direct into-the-turn look, bypassing the spring
    // decimation to DANCE_RATE_HZ
    int decim, decim_n;
    float acc_v, acc_h;
    // ring buffers of vertical and horizontal linear acceleration
    float buf_v[DANCE_SAMPLES], buf_h[DANCE_SAMPLES];
    int buf_pos, buf_filled, since_eval;
    motion_features_t out;
} motion_analysis_t;

void motion_analysis_init(motion_analysis_t *m, float rate_hz);

// Eye offset from the twist axis in its own screen frame: x_m along the
// screen's horizontal (+x), out_m the way the screen faces. Defaults to the
// starboard eye (x = +EYE_FORWARD_M).
void motion_analysis_set_mount(motion_analysis_t *m, float x_m, float out_m);

// acc in g, gyro in deg/s, both in the IMU's own axes.
void motion_analysis_update(motion_analysis_t *m, const float acc[3], const float gyro[3]);
