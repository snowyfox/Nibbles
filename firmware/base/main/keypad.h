// The keypad boards (hardware/keypad): TCA9534A expanders at 0x38-0x3F on
// the power board's I2C bus, read every KEYPAD_POLL_MS and debounced; their
// SK6812 LEDs chained from PIN_KEY_LEDS, then the mirror panel (KEYPAD_PANEL_*).
// Which physical key is which logical key, and the boards' order on the LED
// chain, are in config.h (KEYPAD_BOARDS).
#pragma once

#include <stdbool.h>
#include "controls.h"
#include "esp_err.h"

typedef void (*keypad_cb_t)(ctl_key_t key, bool down);

// The expander on a key board (the chip field of KEYPAD_BOARDS).
#define KEYPAD_TCA9534  0   // TCA9534(A): the real key boards; LEDs on the chain
#define KEYPAD_PCF8574  1   // PCF8574(T/AT) module, for breadboard prototypes: no LEDs

esp_err_t keypad_start(keypad_cb_t cb);
// Each logical key's colour (0xRRGGBB); the driver scales it to KEYPAD_LED_LEVEL.
void keypad_set_leds(const uint32_t rgb[KEY_COUNT]);
// Bit per KEYPAD_BOARDS entry: boards answering on the bus.
uint8_t keypad_boards_found(void);
int keypad_board_count(void);   // boards listed in KEYPAD_BOARDS
// The console's `keymap` commands and `keys` (true if the line was one):
// change which key each switch is, saved in NVS so it outlasts restarts and
// firmware updates (KEYPAD_BOARDS is the default). Answers with KEYMAP lines,
// each passed to reply (NULL: printed on the USB serial). The base also takes
// these over the radio from a brain's /keys page (NL_MSG_CONSOLE).
//   keys                       every key name, space-separated
//   keymap                     every board: number, address, its switches' keys
//   keymap B                   one board (B: 1-N in KEYPAD_BOARDS order, or 0x38...)
//   keymap B S NAME            switch S (1-8) of board B becomes key NAME (as `keys`
//                              lists them; none: no key)
//   keymap B name,name,...     the whole board at once
//   keymap B reset             board B back to KEYPAD_BOARDS
//   keymap reset               every board back to KEYPAD_BOARDS
//   keymap colors              KEYCOLORS B rrggbb,...: each switch's LED colour now
//   keymap last                KEYLAST B S NAME MS: the switch pressed last, MS ago
//                              (KEYLAST none before the first press)
//   keymap all                 every KEYMAP and KEYCOLORS line, KEYFOUND 1,0,...
//                              (which boards answer), and KEYLAST
typedef void (*keypad_reply_t)(void *ctx, const char *line);
bool keypad_console(const char *line, keypad_reply_t reply, void *ctx);
