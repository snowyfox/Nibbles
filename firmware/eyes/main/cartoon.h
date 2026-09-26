// Cartoon / anime eye animations: flat colours, bold outlines, highlights.
// Not driven by sound or motion; each runs on its own clock. Pure C (no
// ESP-IDF) so frames can be rendered and checked on the host.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CARTOON_COUNT 5

// Build the lookup maps (about 870 KB). alloc is used for the large ones
// (PSRAM on the eye). Returns false if memory runs out.
bool cartoon_init(void *(*alloc)(size_t size));

const char *cartoon_name(int anim);

// Set up one frame of animation anim at time t (seconds). mirror flips the
// asymmetric details (highlights, blush) for the other side's eye.
void cartoon_prepare(int anim, float t, bool mirror);

// Draw rows [y0, y1) of the prepared frame, DISP_W pixels each, as RGB565
// big-endian (the panel's byte order). Safe to call from two cores at once
// on different rows.
void cartoon_rows(uint16_t *dst, int y0, int y1);
