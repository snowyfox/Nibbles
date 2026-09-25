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

// Touch actions. Cards are tapped; bump pads report press and release.
typedef enum {
    DISPLAY_TAP_EYES = 1,     // tap the EYES card
    DISPLAY_TAP_WLED,         // tap the WLED card
    DISPLAY_PAD_FLASH,        // hold the FLASH pad
    DISPLAY_PAD_BLACKOUT,     // hold the BLACKOUT pad
    DISPLAY_SLIDE_EYES,       // eyes brightness slider released: value 0..255
    DISPLAY_SLIDE_WLED,       // WLED brightness slider released: value 0..255
    DISPLAY_HOLD_EYES,        // long press on the EYES card (toggle automatic preset changes)
    DISPLAY_TAP_PRESET,       // preset grid: tap button value (1..10) -> that preset on WLED and the eyes
    DISPLAY_HOLD_PRESET,      // preset grid: held (pressed) / let go; value = preset to bump (11..20)
    DISPLAY_SET_AUTO,         // preset page switch: value 1 = eyes change presets by themselves
    DISPLAY_SET_REACT,        // preset page selector: value = nl_react_t
} display_action_t;

// Called on the display task: must not block (hand work to another task).
typedef void (*display_action_cb_t)(display_action_t action, bool pressed, int value);

#define DISPLAY_PRESET_HOLD_OFFSET 10  // holding grid button N bumps preset N + 10

esp_err_t display_start(display_action_cb_t on_action);
// Switch pages (0 = status, 1 = preset grid), e.g. from the serial console.
void display_show_page(int page);
// Print the next frame over the console as base64 RGB565 ("SHOT w h" ...
// "SHOT END"), for checking the layout from a computer.
void display_screenshot(void);
void display_update(const display_status_t *s);
// A one-line message for the event strip (the last command's result, etc.).
void display_event(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
