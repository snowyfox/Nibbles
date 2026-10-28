// The keypad boards (hardware/keypad): TCA9534A expanders at 0x38-0x3F on
// the power board's I2C bus, read every KEYPAD_POLL_MS and debounced; their
// SK6812 LEDs chained from GPIO 43. Which physical key is which logical key,
// and the boards' order on the LED chain, are in config.h (KEYPAD_BOARDS).
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
