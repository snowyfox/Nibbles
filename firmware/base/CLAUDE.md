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
- Logs the eyes' and WLED's telemetry every 2 s.
- BOOT = next eye preset. Second key (GPIO 16) = a **flash bump** for the
  eyes and WLED (`BTN_BUMP_TARGET`) while held (START, HOLD every 100 ms, STOP; WLED releases by itself 300 ms after
  the last message).
- USB serial commands, for testing: `next`, `prev`, `set N` (eyes);
  `wled next`, `wled set N`; `[eyes|wled] bump flash MS`,
  `[eyes|wled] bump black MS`, `[eyes|wled] bump preset N MS` (no prefix:
  flash and blackout go to both, preset bumps to WLED, as the preset numbers
  differ). Commands are unicast with an ack and up to 3 tries of
  60 ms.

Build: `idf.py -C firmware/base build`, flash to the base board's port
(check its serial number, 28:84:85:91:27:08, first).
