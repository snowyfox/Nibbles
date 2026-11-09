// Base station settings. Today this is a prototype on a Waveshare
// ESP32-S3-Touch-LCD-3.49; see docs/architecture.md (Phases 2, 4, 5).
#pragma once

#define BASE_FW_BUILD        2

// 1: always anchor the radio channel. 0: scan for WLED's anchor like the eyes
// (with the fallback below).
#define BASE_IS_ANCHOR       0
// Scanning base: if no anchor (a brain) is heard for this long, the base
// leads a search for one, starting at BASE_ANCHOR_CHANNEL; the eyes and the
// preview screen search with it (NL_HB_GROUP, 2026-11-09).
#define BASE_ANCHOR_FALLBACK_MS 12000
#define BASE_ANCHOR_CHANNEL  6

// Site survey page: listen this long on each channel; recommend only channels
// 1..BASE_MAX_CHANNEL (11: the US); a move happens this long after the tap.
#define SURVEY_DWELL_MS      300
#define BASE_MAX_CHANNEL     11
#define MOVE_DELAY_MS        1000

// Buttons (active low): BOOT = next eye preset; the second key is a bump
// button: the eyes and WLED flash while it is held. The touch screen's FLASH
// and BLACKOUT pads bump the same targets.
#define BTN_NEXT_GPIO        0
#define BTN_BUMP_TARGET      (NL_TARGET_EYES | NL_TARGET_WLED)  // who the bump key flashes
#define BTN_BUMP_GPIO        16

// ---------------------------------------------------------------- mirror board
// A second 3.49 up the pole shows the base's screen and gets its firmware
// from this board over a cable (mirror_link.c, 2026-11-02). Same firmware;
// "role mirror" over USB makes a board the mirror. The main board's end is
// off until "mirror link on" (it takes GPIO 0, the BOOT key, and GPIO 4).
// The 3.49 has no spare header pin free of parts: GPIO 0 has C58 (100 nF,
// the BOOT key's) and GPIO 4 has C10 (100 nF, the battery ADC's) to ground,
// which round a fast signal off. Remove both on the main board for 1 Mbaud
// (or drop MIRROR_BAUD to 19200, slow for updates: ~15 min).
#define MIRROR_UART          1
#define MIRROR_BAUD          1000000
#define MIRROR_MAIN_TX_GPIO  0      // main board: to the mirror's RX (header A1)
#define MIRROR_MAIN_RX_GPIO  4      // main board: from the mirror's TX (header A5)
#define MIRROR_SIDE_TX_GPIO  43     // mirror board: to the main's RX (header B5)
#define MIRROR_SIDE_RX_GPIO  44     // mirror board: from the main's TX (header B6)
#define MIRROR_HELLO_MS      2000   // the main asks the mirror for its build this often

// Status screen: LV_DISPLAY_ROTATION_0 or _180 = portrait (cards stacked),
// _90 or _270 = landscape (cards side by side); each pair picks which way up.
#define DISPLAY_ROTATION     LV_DISPLAY_ROTATION_0
#define DISPLAY_BACKLIGHT_PCT 80

#define CMD_TRIES            3
#define CMD_TIMEOUT_MS       60
#define TAKE_FADE_MS         0       // two brains: LATCH on a held TAKE is a cut; a fade would need a separate key

// ---------------------------------------------------------------- power board
// Its own I2C bus on the 3.49's header (J6 A2-A4 on the power board), dock
// detect on A6. 100 kHz: the keypads share the bus, one of them 60-80 cm away.
#define PIN_PWR_SDA          1
#define PIN_PWR_SCL          2
#define PIN_PWR_INT          3
#define PIN_DOCK_DET         5

// The TF card, in 1-bit SD mode (Waveshare's schematic and factory demo).
#define PIN_SD_CLK           41
#define PIN_SD_CMD           39
#define PIN_SD_D0            40

// The PCF85063 clock (with the board's IMU and other sensors, which we don't use).
#define PIN_RTC_SDA          47
#define PIN_RTC_SCL          48
#define POWER_I2C_HZ         100000
#define POWER_POLL_MS        200
#define BASE_LOAD_MW         2000    // the base itself, for the runtime estimate (estimate)

// Charger (BQ25798) settings, written at start-up and read back (charging is
// switched off if they don't stick). 4S at 4.2 V per cell; 2.0 A is the
// Vapcell F63's standard charge. CHG_IINDPM_MA 0 keeps the input limit the
// board's ILIM_HIZ divider sets (2.0 A, so lights plus charging stay inside
// a VB99 D-Tap's 10 A); a value (100-3300) replaces it.
#define CHG_VREG_MV          16800
#define CHG_ICHG_MA          2000
#define CHG_IINDPM_MA        0

// How often the base tells WLED the pack voltage and the lights' current
// (NL_MSG_BATTERY, for its battery-aware current limit). Simulated readings are never sent.
#define BATTERY_BROADCAST_MS 200

// ---------------------------------------------------------------- keypad
// Key boards on the power board's bus (TCA9534A, address set by each board's
// jumpers) in the order their LEDs are chained (hub J2, J3, ...). Each lists
// its switches top to bottom (switch 1 = P0 = its first LED); KEY_NONE for
// a switch with no job. See controls.h for what the keys do.
// The last entry is the breadboard prototype: a PCF8574T module (no LEDs;
// switches between a pin and GND, its pins pull themselves up) at 0x27, all
// three address jumpers high (PCF8574T: 0x20-0x27; PCF8574AT: 0x38-0x3F).
// A board that isn't there is just never found, so the entries can stay.
#define KEYPAD_BOARDS { \
    { 0x38, KEYPAD_TCA9534, 8, { KEY_NUM1, KEY_NUM2, KEY_NUM3, KEY_NUM4, KEY_NUM5, KEY_NUM6, KEY_NUM7, KEY_NUM8 } }, \
    { 0x39, KEYPAD_TCA9534, 8, { KEY_ALL, KEY_BODY, KEY_EYES, KEY_FINS, KEY_PAGE, KEY_LATCH, KEY_BLACKOUT, KEY_FIN_SENSOR } }, \
    { 0x3A, KEYPAD_TCA9534, 3, { KEY_TAKE, KEY_FLASH, KEY_UNLOCK } }, \
    { 0x27, KEYPAD_PCF8574, 8, { KEY_NUM1, KEY_NUM2, KEY_NUM3, KEY_NUM4, KEY_EYES, KEY_BODY, KEY_LATCH, KEY_TAKE } }, \
}
#define KEYPAD_POLL_MS       10
// The key LEDs' data: GPIO 43 (U0TXD; the console is on USB), wired to the
// hub's J1 pin 7. The ROM prints its boot banner on this pin, so the LEDs
// may flicker for a moment at power-up until the firmware sets them.
#define PIN_KEY_LEDS         43
// Scale for the key LEDs (0-255): 19 SK6812s at full white would draw ~1.1 A
// from the keypad cable's 5 V; 48 keeps it under ~0.25 A and is plenty at night.
#define KEYPAD_LED_LEVEL     48
// Lock the keypad after this long without a key press (0: only by holding UNLOCK).
#define KEYPAD_AUTOLOCK_MS   0
