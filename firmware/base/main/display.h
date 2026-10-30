// Status screen on the 3.49's 172x640 AXS15231B panel, in portrait, with
// LVGL. Page 0 is the base's main screen (power, Live / Standby, the
// keypad's target); swipe for the preset grid (1) and the radio details (2).
// main.c hands it a status snapshot a few times a second; all LVGL calls stay
// on the display task.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "controls.h"
#include "nibbles_link.h"
#include "power.h"

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
    // showing a stream from another WLED, and how steadily it arrives (NL_MSG_WLED_STREAM)
    bool stream_fresh;
    nl_wled_stream_t stream;
    // a bump being held right now (0 = none)
    uint8_t bump_action;
    // whether WLED's fin LEDs follow the fins (NL_MSG_FIN_STATE)
    bool fins_fresh;
    nl_fin_state_t fins;
    // power board
    power_status_t power;
    // keypad: Live / Standby and the keys' state (controls.c)
    ctl_scene_t live, standby;
    char live_body[NL_NAME_LEN], live_eyes[NL_NAME_LEN];   // names, "" where not known
    char stby_body[NL_NAME_LEN], stby_eyes[NL_NAME_LEN];
    char live_fins[NL_NAME_LEN], stby_fins[NL_NAME_LEN];  // fin preset names
    ctl_layers_t layers;      // WLED's layer slots (palettes, overlays), -1 where not known
    char live_pal_name[24];   // the body's palette by name, "" if not one the keypad knows
    int8_t fin_sensor;        // the fins follow their angle sensor (1), are locked (0), -1 not known
    uint8_t brightness;       // WLED's master brightness
    uint8_t target;           // ctl_target_t
    uint8_t page;
    uint8_t diff;             // ctl_standby_diff()
    bool keys_locked, take_preview, latch;
    uint8_t keypads_found, keypads_total;
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
    DISPLAY_SET_FINS,         // preset page switch: value 1 = WLED's fin LEDs follow the fins' angle
    DISPLAY_KEY,              // main page: a keypad key pressed / released by touch; value = ctl_key_t
} display_action_t;

// Called on the display task: must not block (hand work to another task).
typedef void (*display_action_cb_t)(display_action_t action, bool pressed, int value);

#define DISPLAY_PRESET_HOLD_OFFSET 10  // holding grid button N bumps preset N + 10

esp_err_t display_start(display_action_cb_t on_action);
// Switch pages (0 = main, 1 = preset grid, 2 = details), e.g. from the serial console.
void display_show_page(int page);
// Print the next frame over the console as base64 RGB565 ("SHOT w h" ...
// "SHOT END"), for checking the layout from a computer.
void display_screenshot(bool rle);   // rle: run-length encoded ("SHOT w h rle565le")
void display_update(const display_status_t *s);
// A one-line message for the event strip (the last command's result, etc.).
void display_event(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
