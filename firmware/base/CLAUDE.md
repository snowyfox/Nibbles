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
  (`BASE_IS_ANCHOR 0`). Set it to 1 to make the base the anchor on channel 6
  when no WLED usermod is around.
- Logs the eyes' and WLED's telemetry every 2 s.
- BOOT = next eye preset. Second key (GPIO 16) = a WLED **flash bump** while
  held (START, HOLD every 100 ms, STOP; WLED releases by itself 300 ms after
  the last message).
- USB serial commands, for testing: `next`, `prev`, `set N` (eyes);
  `wled next`, `wled set N`; `bump flash MS`, `bump black MS`,
  `bump preset N MS`. Commands are unicast with an ack and up to 3 tries of
  60 ms.

Build: `idf.py -C firmware/base build`, flash to the base board's port
(check its serial number, 28:84:85:91:27:08, first).
