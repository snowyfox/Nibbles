// The mirror panel's layout (KEYPAD_PANEL_* in config.h): which key board and
// switch each panel pixel repeats. Kept apart from keypad.c so the host tests
// can check it.
#pragma once

#include <stdbool.h>

// Pixel `pixel` of a panel with `row_len` pixels per row and `pixels` in all:
// sets *board (index among the LED boards, chain order) and *sw (0 = switch 1).
// Returns false for a pixel outside the panel.
static inline bool keypad_panel_source(int pixel, int pixels, int row_len, bool flip_rows, bool flip_keys,
                                       int *board, int *sw)
{
    if (pixel < 0 || pixel >= pixels || row_len <= 0) return false;
    const int rows = (pixels + row_len - 1) / row_len;
    const int row = pixel / row_len, col = pixel % row_len;
    *board = flip_rows ? rows - 1 - row : row;
    *sw = flip_keys ? row_len - 1 - col : col;
    return true;
}
