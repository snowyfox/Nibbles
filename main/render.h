// Neon eye renderer. Draws straight to the CO5300 panel in strips; no LVGL.
#pragma once

#include "esp_err.h"
#include "eye.h"

esp_err_t render_init(void);

// Render and send one full frame. Call from a single task only; it owns the
// display bus.
void render_frame(const eye_params_t *p);

// Since the last call: frames that took too long to send (and may have torn),
// and the slowest send time measured from the refresh edge.
void render_take_stats(int *late, float *worst_ms);

// Must be called from the same task as render_frame.
void render_set_brightness(int percent);
