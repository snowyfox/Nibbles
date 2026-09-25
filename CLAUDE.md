# Nibbles

A festival totem: a shark on top of a 3 m pole that looks like it swims in the
sky and dances to the music (trance, EDM, dubstep, drum and bass; up to 180+
bpm). Several ESP32 boards work together as one system. This repo holds all
of Nibbles' own code; WLED itself stays upstream.

## The pieces

| Part | Hardware | Code | Status |
|---|---|---|---|
| **Eyes** (port + starboard) | 2× Waveshare ESP32-S3-Touch-AMOLED-1.75, back to back | `firmware/eyes/` (ESP-IDF 5.5.5) | Working; linked by a 3-wire cable (starboard leads, port is the ears); leader on the radio: telemetry, preset and brightness commands, bumps |
| **Lights** | Existing ESP32 running WLED (0.15, to be upgraded to 16.x), external digital mic | `wled/usermod_nibbles/` + `wled/platformio_override.ini` (see `wled/README.md`) | Usermod works on a dev node (WLED 16.0.1): channel anchor, preset/brightness commands, bumps, telemetry, optional AudioReactive features; the real controller (WT32-ETH01) now runs it too |
| **Base station** | New ESP32 in the pole base: batteries, status screen(s), buttons | `firmware/base/` (ESP-IDF 5.5.5) | Prototype on a Waveshare ESP32-S3-Touch-LCD-3.49: LVGL touch status screen (eyes, WLED, radio; tap for presets, brightness sliders, FLASH/BLACKOUT pads), BOOT = next eye preset, second key = flash bump; scans for WLED and anchors the channel itself if WLED is absent |
| **Shared protocol** | used by all of the above | `shared/nibbles_link/` | Framing, radio packets, eye messages, commands, bumps, channel scanning with fallback anchoring, WLED audio conversion; host tests (`make -C shared/nibbles_link`) |
| **Radio transport** | ESP-IDF boards (eyes, base) | `shared/nibbles_espnow/` | ESP-NOW: anchor or scan, broadcast, commands with ack/retry |

`docs/architecture.md` has the agreed design and roadmap: roles, the wired eye
link, ESP-NOW with WLED as the channel anchor, the message rules, and phases.
Each sub-project has its own `CLAUDE.md` with details, loaded automatically
when working in that directory. The eyes' one is the most complete record of
how the firmware works and why.

## Boards

| Board | MAC / USB serial | Role |
|---|---|---|
| Starboard eye | 28:84:85:3A:DA:78 | link leader (shared eye brain) and radio gateway |
| Port eye | 28:84:85:3B:6F:F4 | recognised by MAC (`PORT_EYE_MACS`); the ears (mic + audio analysis) |
| Base prototype | 28:84:85:91:27:08 | ESP32-S3-Touch-LCD-3.49 running `firmware/base` |

Eye cable (8-pin headers): GND↔GND (pin 2), starboard GPIO 17 (pin 6) → port GPIO 18
(pin 7), port GPIO 17 → starboard GPIO 18. Never link VBUS (pin 1) or 3V3.

The USB port name varies (`/dev/cu.usbmodem201`, `…401`): `ls /dev/cu.usbmodem*`.
Check which board is connected by its serial number before flashing
(`system_profiler SPUSBDataType | grep -A6 JTAG`).

## Common commands

```sh
source ~/.espressif/tools/activate_idf_v5.5.5.sh     # ESP-IDF (EIM install; export.sh does not work here)
make -C firmware/eyes/test/host                       # eye host tests (no hardware)
make -C shared/nibbles_link                           # link protocol host tests
idf.py -C firmware/eyes build
idf.py -C firmware/eyes -p /dev/cu.usbmodemNNN flash
idf.py -C firmware/base build                         # base prototype; flash the same way
```

Testing radio commands without pressing buttons: write commands (plus
newline) to the base prototype's USB serial port: `next`, `prev`, `set N`,
`wled next`, `wled set N`, `bri N`, `wled bri N`, `auto on|off`, `preset N`, `page 0|1`, `react preset|beats|peaks`, `[eyes|wled] bump
flash|black MS`, `[eyes|wled] bump preset N MS`, and `shot` (a screenshot of
its display as base64 RGB565; see `firmware/base/CLAUDE.md`). WLED builds are
in `wled/README.md`.

## Conventions

- Only commit when asked, and **push every commit to `origin`**
  (github.com/snowyfox/Nibbles, branch `master`) right after committing.
  Commits are **unsigned**: this repo's local git
  config sets `commit.gpgsign` and `tag.gpgsign` to false (the user's global
  config signs by default), so a plain `git commit` works here.
- Work in small verified steps: host tests, then build, then flash, then read
  the board's once-a-second status log. The user checks anything visual or
  physical; ask them to confirm.
- Prefer measuring on real hardware and real recordings over guessing (see the
  recording and replay tools in `firmware/eyes/CLAUDE.md`).
- Anything that more than one board must agree on goes in the shared protocol,
  not copied into each firmware.
