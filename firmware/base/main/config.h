// Base station settings. Today this is a prototype on a Waveshare
// ESP32-S3-Touch-LCD-3.49; see docs/architecture.md (Phases 2, 4, 5).
#pragma once

#define BASE_FW_BUILD        2

// 1: always anchor the radio channel. 0: scan for WLED's anchor like the eyes
// (with the fallback below).
#define BASE_IS_ANCHOR       0
// Scanning base: if no anchor (the WLED usermod) is heard for this long, the
// base anchors BASE_ANCHOR_CHANNEL itself until it finds one.
#define BASE_ANCHOR_FALLBACK_MS 12000
#define BASE_ANCHOR_CHANNEL  6

// Buttons (active low): BOOT = next eye preset; the second key is a bump
// button: the eyes and WLED flash while it is held. The touch screen's FLASH
// and BLACKOUT pads bump the same targets.
#define BTN_NEXT_GPIO        0
#define BTN_BUMP_TARGET      (NL_TARGET_EYES | NL_TARGET_WLED)  // who the bump key flashes
#define BTN_BUMP_GPIO        16

// Status screen: LV_DISPLAY_ROTATION_0 or _180 = portrait (cards stacked),
// _90 or _270 = landscape (cards side by side); each pair picks which way up.
#define DISPLAY_ROTATION     LV_DISPLAY_ROTATION_0
#define DISPLAY_BACKLIGHT_PCT 80

#define CMD_TRIES            3
#define CMD_TIMEOUT_MS       60
