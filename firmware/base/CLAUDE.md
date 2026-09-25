# Nibbles base station firmware (prototype)

The box in the pole base: in the end it manages the batteries, shows system
status on its screen(s), and has buttons to control the eyes and WLED
(including DMX-style bump buttons). See the root `CLAUDE.md` and
`docs/architecture.md` (Phases 4 and 5).

Today it is a prototype on a **Waveshare ESP32-S3-Touch-LCD-3.49** (ESP32-S3R8,
16 MB flash, 8 MB octal PSRAM, 172×640 AXS15231B QSPI screen, BOOT = GPIO 0,
second key = GPIO 16, QMI8658, PCF85063, ES8311/ES7210, TCA9554, battery
charger). Demo code: https://github.com/waveshareteam/ESP32-S3-Touch-LCD-3.49
(screen pins: CS 9, PCLK 10, DATA0-3 11-14, RST 21, backlight 8; touch I2C
SDA 17 / SCL 18 at 0x3B; sensors I2C SDA 47 / SCL 48).

What it does now (`main/main.c`):
- Scans for the WLED usermod's anchor heartbeat like the eyes
  (`BASE_IS_ANCHOR 0`). If none is heard for 12 s (`BASE_ANCHOR_FALLBACK_MS`)
  it anchors channel 6 itself, so the eyes still have a channel. It then spends
  300 ms on another channel every 3 s, in turn, looking for WLED, and hands
  over as soon as it hears it. Commands and bumps end a peek at once and put
  the next one off by 3 s, so held bumps never miss keepalives.
  `BASE_IS_ANCHOR 1` makes it a fixed anchor.
- **Status screen** (`main/display.c`, LVGL 9.2 + `esp_lcd_axs15231b` from the
  component manager): portrait 172×640 by default (`DISPLAY_ROTATION` 0 or
  180; cards stacked, big FLASH/BLACKOUT pads along the bottom) or landscape
  640×172 (90 or 270; cards side by side). Preset names use a 20 px font and
  wrap (portrait) or end in "..." (landscape), never scroll. Three cards (EYES: preset, state,
  link, both fps, bpm, hype bar; WLED: on/preset, brightness, effect, palette,
  fps, LEDs, brightness bar; RADIO: channel, anchoring/locked/scanning,
  counters) and an event line (last command result, held bump, which also
  outlines the screen). Stale cards grey out after 2 s. `DISPLAY_ROTATION`
  picks the layout and which way up. Panel notes: frames go out whole in 64-row QSPI chunks
  (the AXS15231B takes whole frames in order); LVGL renders full-frame in
  the chosen orientation and `flush_cb` copies (rotation 0) or rotates into a separate buffer and byte-swaps that
  (never the LVGL buffer, which LVGL keeps between frames); set the rotation
  before `lv_display_set_buffers` (the stride comes from the width); LVGL's
  printf has no `%f`.
- **Touch** (AXS15231B touch at 0x3B on I2C SDA 17 / SCL 18, read with the
  vendor's 11-byte command; native coordinates, LVGL rotates them): tap the
  EYES or WLED card for that side's next preset; long-press the EYES card to
  hold the current preset or let them change by themselves again (the card
  shows `auto` or `held`); drag the slider at the
  bottom of either card to set its brightness (sent on release); hold the FLASH or BLACKOUT
  pad to bump (targets `BTN_BUMP_TARGET`; BLACKOUT wins over FLASH). Taps go
  through a queue to an actions task, since commands block for their ack; the
  buttons task runs one bump state machine for the key and both pads.
- **Preset page** (swipe left; swipe right to go back; an LVGL tileview):
  square buttons 1-10, 2 columns by 5 rows in portrait (5 by 2 in landscape).
  Tap N sets preset N on WLED (preset id N) and the eyes (index N-1). Hold N
  bumps preset N+10 on both while held (`DISPLAY_PRESET_HOLD_OFFSET`). The
  eyes have 18 presets, so holds of 9 and 10 only change WLED. At the top:
  an AUTO switch (eyes change presets by themselves; turns off when a preset
  is picked) and a Preset / Beats / Peaks selector (eye sound reactivity:
  each preset's choice, or all on beats / on every peak). Both follow the
  eyes' telemetry flags, but not for 1.5 s after being touched. The sliders
  don't start a page swipe (they don't chain scrolling to the tileview).
- `shot` over USB serial (or `python tools/base_shot.py PORT out.png`) prints the next frame as base64 RGB565 (`SHOT 640 172
  rgb565le` … `SHOT END`) to check the layout from a computer.
- Logs the eyes' and WLED's telemetry every 2 s.
- BOOT = next eye preset. Second key (GPIO 16) = a **flash bump** for the
  eyes and WLED (`BTN_BUMP_TARGET`) while held (START, HOLD every 100 ms, STOP; WLED releases by itself 300 ms after
  the last message).
- USB serial commands, for testing: `next`, `prev`, `set N` (eyes);
  `wled next`, `wled set N`; `bri N`, `wled bri N` (0..255, or `+N`/`-N` to
  step); `auto on|off`; `preset N` (as a grid tap); `page 0|1`; `react preset|beats|peaks`; `shot`; `[eyes|wled] bump flash MS`,
  `[eyes|wled] bump black MS`, `[eyes|wled] bump preset N MS` (no prefix:
  flash and blackout go to both, preset bumps to WLED, as the preset numbers
  differ). Commands are unicast with an ack and up to 3 tries of
  60 ms.

Partition table: ESP-IDF's "single app, large" (1.5 MB app); the app outgrew
the default 1 MB one when the preset page was added.

Build: `idf.py -C firmware/base build`, flash to the base board's port
(check its serial number, 28:84:85:91:27:08, first).
