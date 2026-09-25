# Nibbles WLED integration

The shark's other lights run stock **WLED**, not a fork. Nibbles adds one
usermod, `usermod_nibbles/`, built into WLED through PlatformIO's
`custom_usermods` from this folder. It uses the same protocol library as the
eyes and base (`shared/nibbles_link`).

## What the usermod does
- **Turns ESP-NOW on** at startup (WLED defaults it off) and acts as the
  **channel anchor**: it broadcasts anchor heartbeats 4 times a second on
  whatever channel WLED is on (a phone hotspot at home, its own AP at a
  festival). The eyes and base scan until they hear one and move to the
  channel it advertises.
- Takes Nibbles **commands aimed at WLED** (`NL_TARGET_WLED`: preset set,
  next, previous) and acks them. A retry with the same id is acked but applied once.
- Broadcasts **WLED telemetry** twice a second (on, brightness, preset,
  effect, palette, channel, fps, LED count).
- Shows the radio status and the **eyes' telemetry on WLED's Info page**.
- Leaves other ESP-NOW traffic (e.g. WiZ remotes) to WLED.
- Settings (WLED Usermods page, `Nibbles`): `enabled`, `anchor`.

## Building
Tested with **WLED v16.0.1**, PlatformIO 6.2.0 and Node.js 20 (WLED's web UI
build needs a current Node; the system Node 8 is too old).

```sh
git clone --depth 1 --branch v16.0.1 https://github.com/wled/WLED.git WLED-16
cd WLED-16
ln -s /Volumes/Code/Nibbles/wled/platformio_override.ini platformio_override.ini
npm ci
NIBBLES_ROOT=/Volumes/Code/Nibbles pio run -e nibbles_dev_s3_349            # build
NIBBLES_ROOT=/Volumes/Code/Nibbles pio run -e nibbles_dev_s3_349 -t upload --upload-port /dev/cu.usbmodemNNN
```

`platformio_override.ini` defines:
- `nibbles_dev_s3_349`: a development node on a Waveshare
  ESP32-S3-Touch-LCD-3.49 (16 MB flash, 8 MB octal PSRAM). LED data on GPIO 2,
  button on BOOT (GPIO 0), no relay or IR, `NIBBLES_DEBUG` on (a status line
  over USB serial every 5 s, and one per command). AudioReactive is included
  (WLED's default for this env).

Build notes:
- The usermod's library name must start with `wled-` (`wled-nibbles`) or be
  given as `wled-nibbles = symlink://…`, or WLED's build script won't treat it
  as a usermod and `wled.h` won't be found.
- `shared/nibbles_link/library.json` makes the protocol a PlatformIO library;
  WLED builds with `lib_compat_mode = strict`, so it declares all frameworks
  and platforms.

## The shark's real controller
The existing controller appears (from the user's WLED 0.15.0-b3 build
settings) to be an **ESP32-S3 on a Lolin S3 Mini** (4 MB flash, 2 MB PSRAM)
with **AudioReactive**; WLED 16 has a matching `esp32s3_4M_qspi` env. Before
upgrading it: back up its config and presets (WLED → Config → Security &
Updates → Backup), confirm the board and pins, then add a
`nibbles_shark` env here extending `esp32s3_4M_qspi`.

Known WLED issue to watch (fixed only in 17.0.0-dev): "ESP-NOW remote with no
Wi-Fi reboots every 15–20 min". Test for it in AP-fallback mode.
