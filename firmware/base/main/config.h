// Base station settings. Today this is a prototype on a Waveshare
// ESP32-S3-Touch-LCD-3.49; see docs/architecture.md (Phases 2, 4, 5).
#pragma once

#define BASE_FW_BUILD        1

// Until the WLED usermod exists, the base anchors the radio channel. Once WLED
// anchors, set this to 0 and the base scans for WLED like the eyes do.
#define BASE_IS_ANCHOR       0
#define BASE_ANCHOR_CHANNEL  6

// Buttons (active low): BOOT = next eye preset; the second key is a bump
// button: WLED flashes white while it is held.
#define BTN_NEXT_GPIO        0
#define BTN_BUMP_GPIO        16

#define CMD_TRIES            3
#define CMD_TIMEOUT_MS       60
