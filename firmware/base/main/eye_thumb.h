// A still picture of an eye for the base's screen: the same rings, spiral arms,
// colours and lids as the eyes' renderer (firmware/eyes/main/render_core.c), at
// thumbnail size, with no music or motion. Drawn only when the preset or the eye's
// state changes. Plain C, host-tested.
#pragma once

#include <stdint.h>
#include "../../eyes/main/presets.h"

#define EYE_THUMB_SIZE 80

// How far open the lids are for an eye state (eye_state_t, in the telemetry).
float eye_thumb_lid(int state);

// size x size RGB565. spiral_mirror: the port eye's spirals turn the other way.
void eye_thumb_draw(uint16_t *px, int size, const preset_t *p, float lid_open, bool spiral_mirror);

// Eyes not heard: only the outline and pupil, in a dim grey.
void eye_thumb_unlit(uint16_t *px, int size, uint16_t grey);
