// Status screen on the 3.49's 172x640 AXS15231B panel, shown in landscape
// (640x172) with LVGL. main.c hands it a status snapshot a few times a second;
// all LVGL calls stay on the display task.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "nibbles_link.h"

typedef struct {
    // radio
    uint8_t channel;
    bool locked, anchoring;
    uint32_t rx, tx, tx_fail;
    // eyes (from the leader's telemetry)
    bool eyes_fresh;
    nl_eye_telemetry_t eyes;
    // WLED
    bool wled_fresh;
    nl_wled_telemetry_t wled;
    // a bump being held right now (0 = none)
    uint8_t bump_action;
} display_status_t;

esp_err_t display_start(void);
// Print the next frame over the console as base64 RGB565 ("SHOT w h" ...
// "SHOT END"), for checking the layout from a computer.
void display_screenshot(void);
void display_update(const display_status_t *s);
// A one-line message for the event strip (the last command's result, etc.).
void display_event(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
