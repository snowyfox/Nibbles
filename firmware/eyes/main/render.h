// Neon eye renderer. Draws straight to the CO5300 panel in strips; no LVGL.
#pragma once

#include "esp_err.h"
#include "eye.h"

esp_err_t render_init(void);

// Render and send one full frame. Call from a single task only; it owns the
// display bus.
void render_frame(const eye_params_t *p);

// Draws rows [y0, y1) of the frame into dst (DISP_W pixels per row, RGB565
// big-endian for the panel). Called from both cores at once, on different rows.
typedef void (*render_rows_fn)(uint16_t *dst, int y0, int y1);

// Render and send one full frame drawn by another renderer (e.g. the cartoon
// animations), with the same refresh sync and strip pipeline.
void render_frame_rows(render_rows_fn rows);

// Since the last call: frames that took too long to send (and may have torn),
// and the slowest send time measured from the refresh edge.
void render_take_stats(int *late, float *worst_ms, float *worst_prep_ms);

// Select the visual preset (index into presets[], wraps).
void render_set_preset(int index);

// Must be called from the same task as render_frame.
void render_set_brightness(int percent);
