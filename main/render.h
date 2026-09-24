// Neon eye renderer. Draws straight to the CO5300 panel in strips; no LVGL.
#pragma once

#include "esp_err.h"
#include "eye.h"

esp_err_t render_init(void);

// Render and send one full frame. Call from a single task only; it owns the
// display bus.
void render_frame(const eye_params_t *p);

// Must be called from the same task as render_frame.
void render_set_brightness(int percent);
