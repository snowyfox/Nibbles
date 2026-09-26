// The eye's picture, without the panel: build the colour tables for a frame
// from the eye's parameters, then draw rows from them. Pure C (see render.c
// for the panel side, and tools/sim for the simulator).
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "eye.h"

// Builds the distance and spiral maps (several MB; alloc should give PSRAM on
// the eye). Returns false if memory runs out.
bool render_core_init(void *(*alloc)(size_t size));

// Draw spirals mirrored left to right: the port eye's, so the two eyes (which
// face opposite ways) spin and twist as mirror images. Set before init on the
// eye; the simulator switches per eye (the other set is built on first use).
void render_core_set_mirror(bool on);
// Select the visual preset (index into presets[], wraps).
void render_core_set_preset(int index);

// Build this frame's colour tables from the eye's parameters.
void render_core_prepare(const eye_params_t *p);

// Draw rows [y0, y1) into dst (DISP_W pixels per row, RGB565 byte-swapped for
// the panel). Can be called from two cores at once for different rows.
void render_core_rows(uint16_t *dst, int y0, int y1);
