// Setting the clock from a phone over Bluetooth (2026-11-14), for boards with
// no clock chip (the 1.91). While the time isn't known, the base advertises as
// "Nibbles Base"; a phone that connects is paired with (Just Works, no PIN)
// and its standard Current Time Service (0x1805, which iOS provides itself)
// is read. Then the link is dropped and Bluetooth is switched off, so the
// radio is ESP-NOW's alone again. It also switches off after
// BLE_TIME_WINDOW_MS with no phone, or when the time comes another way.
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    BLE_TIME_OFF = 0,        // not running (never started, finished or given up)
    BLE_TIME_WAITING,        // advertising, no phone yet
    BLE_TIME_CONNECTED,      // a phone is connected (pairing, reading)
} ble_time_state_t;

// Starts the window (no-op if it is running or the build has no Bluetooth).
void ble_time_start(void);
// Called from the main loop: hands over a time read from a phone (Unix, UTC;
// true once per reading), and switches Bluetooth off when it's done, the
// window is over, or stop_now (the clock was set another way).
bool ble_time_poll(bool stop_now, uint32_t *unix_time);
ble_time_state_t ble_time_state(void);
