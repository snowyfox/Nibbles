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
  next, previous; brightness set 0..255 or step by a signed amount) and acks
  them. A retry with the same id is acked but applied once.
  The ack goes out before the preset is applied. Which presets exist is cached
  from `/presets.json` (re-read when WLED saves presets), so next/previous skip
  gaps and wrap, and a missing preset is refused (status 2).
- Plays **bumps** (`NL_MSG_BUMP`, broadcast): flash (all segments solid white),
  blackout, or a preset, held while the sender keeps sending HOLD every 100 ms.
  On STOP, or 300 ms without a message, it restores the state (and current
  preset) saved at the start. A new bump replaces the one playing and keeps the
  original saved state. Preset and brightness commands that arrive during a bump
  are applied when it ends (instead of the saved preset), so the release
  doesn't undo them (verified: preset and brightness sent mid-flash took effect
  on release).
- Broadcasts **WLED telemetry** twice a second (on, brightness, preset and its
  name, effect, palette, channel, fps, LED count). Preset names come from the
  same cached read of `/presets.json` as the preset list.
- Shows the radio status and the **eyes' telemetry on WLED's Info page**.
- Leaves other ESP-NOW traffic (e.g. WiZ remotes) to WLED.
- With the `audio` setting on, publishes **AudioReactive's analysis** of the
  controller's mic as Nibbles audio features every 32 ms (`nl_ar_update`), for
  eyes built with `EYES_AUDIO_FROM_WLED 1`. The Info page shows whether it is
  sending. Untested on real music so far.
- **Mirrors the lights to the simulator** (`tools/sim`) on request: a JSON
  state post `{"nibbles":{"mirror":{"ip":"a.b.c.d","port":4050,"s":6}}}`
  starts it for `s` seconds (at most 30; `s: 0` stops it), and `server.py`
  renews it every 2 s while the page shows the live shark. Each shown frame
  goes to `ip:port` as DDP (gamma and brightness applied; the current
  limiter's extra dimming is not), at most 50 a second, and a small JSON
  packet to `port + 1` with WLED's brightness and preset, the audio features
  it last sent the eyes, and the eyes' telemetry. It works even with the
  usermod disabled, and costs nothing when not asked for. Bench-tested on
  the WT32 build: identical to the bench's own network LED output, 44
  frames/s, WLED's frame rate unchanged (43 fps), 10-minute soak clean. On
  the real controller since 2026-09-26 (42 frames/s to the simulator).
- **2D map partners**: with the shark's 2D map (below) it copies each partner
  LED's colour from the LED it pairs with after every frame (the `"copy"`
  list in `/ledmap.json`). The Info page shows how many ("Nibbles 2D map").
- Settings (WLED Usermods page, `Nibbles`): `enabled`, `anchor`, `audio`
  (default off).

## 2D map
`wled/ledmap/ledmap.json` (made by `tools/sim/make_ledmap.py` from the
simulator's layout; preview in `ledmap_preview.svg`) makes WLED treat the
shark as a 70 x 35 matrix, seen from starboard with the nose to the right, for
WLED's 2D effects. Seen from the side, LEDs overlap: the two strips back to
back in the main tube, and the two fins. So only the outward strip (channel 1)
and the starboard fin (channel 5) are mapped where they are (90% within one
9 mm cell of their real spot); the inward strip and the port fin sit in spare
cells nearby, and the usermod copies each one's colour from its partner, so
both sides match. The fins stick out sideways, so from the side each is a compact patch.

To use it: upload the file as `/ledmap.json` (WLED's file editor, or
`curl -F "data=@wled/ledmap/ledmap.json;filename=/ledmap.json" http://IP/upload`)
and reboot. **WLED then runs as a 2D matrix for every preset**: 1D presets
(segments by strip position) need redoing for 2D; a 2D segment covering the
whole matrix is `{"start":0,"stop":70,"startY":0,"stopY":35}`. Delete the
file and reboot to go back.

Bench (2026-09-26, same firmware and map): the partner copies match exactly
(0 of 531 differ), the eye rings stay dark; most 2D effects run at 43 fps
(Octopus, Noise2D, Waverly, Black Hole), Plasma Ball 40, Distortion Waves 24;
the matrix costs 4-9 KB of RAM. **On the shark's controller since
2026-09-26** (with the lean build): 42 fps, 83 KB free; while the simulator
mirrors it, 33 fps and 25-30 frames/s to the simulator. Its 1D presets don't
work in 2D (see above); to go back to 1D, follow
`wled/backup/20260926-1850-pre-2d/REVERT.md` (delete `/ledmap.json`, reboot;
tested on the bench).

## Lean build
`nibbles_shark` leaves out features the shark doesn't use: Ethernet (it runs
on Wi-Fi; Ethernet was already off in its config), Alexa, Hue sync, MQTT,
infrared, Loxone and the Pixel Forge tool. That saves 82 KB of program space
(1.25 MB of the 1.5 MB app slot used, ~318 KB free) but only ~1.3 KB of static
RAM and 1.5-3 KB of free heap on the bench: most RAM goes to Wi-Fi,
AudioReactive, WLED's JSON buffer and the LED buffers, which these flags
don't touch. The release name stays "ESP32_Ethernet": WLED 16 refuses an
over-the-air update whose release name differs from the installed one.

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
A **Bong69 8 Port LED Distro v3** (https://github.com/bobko69/8PortLEDDistro):
a WT32-ETH01 (classic ESP32, 4 MB flash, no PSRAM; LAN8720 clocked by an
external oscillator on GPIO 0, so ESP-NOW is safe; Ethernet is off in its
config anyway). Read on 2026-09-24 at 10.7.200.253: WLED 0.15.1
"ESP32_Ethernet" release, 1194 LEDs on 5 WS281x outputs (config pins 1-5, matching the board's silkscreen: channels 1-5 = GPIO 1-5, channels 6-8 = GPIO 12, 14, 15 (unused); all working;
**outputs changed 2026-09-27, see "LED outputs now" below**),
button on GPIO 0, AudioReactive with an I2S mic (SD 17, WS 32, SCK 33),
ESP-NOW on with a linked WiZmote, 42 presets (`presets.json` is 87 KB, so the
usermod reads it with a names-only filter), Wi-Fi channel 11 at home, AP
"Nibbles" on channel 1. A backup of its `cfg.json`, `presets.json` and the
0.15.1 release image for rolling back is in `wled/backup/` (not committed).

**LED outputs now (2026-09-27).** The fins moved from channels 3/4 to 4/5,
and last year's eye lights (130 LEDs, behind the new eye screens) were
removed, so channel 3 (GPIO 3, driven by the board's USB chip) is free and
the controller's USB can stay connected, e.g. to a hub in the box:

| Output | Pin | LEDs | What |
|---|---|---|---|
| 1 | GPIO 1 | 0-417 | main tube, outward strip |
| 2 | GPIO 2 | 418-834 | main tube, inward strip |
| 3 | - | - | unused (the CH340 drives GPIO 3) |
| 4 | GPIO 4 | 835-948 | port fin |
| 5 | GPIO 5 | 949-1063 | starboard fin |

1064 LEDs; the LED numbers didn't change, so presets, the 2D map and the
simulator layout still match. Of the spare channels 6-8, use 7 (GPIO 14) or
8 (GPIO 15) first: GPIO 12 (channel 6) sets the flash voltage at boot and
must not be pulled high then. Backup from before the change:
`wled/backup/20260927-0851-pre-channel-move/`.

**`nibbles_shark`** env: WLED 16's `esp32_eth` plus the usermod. It builds and
fits: 1.33 MB of the 1.5 MB app partition.

**First upgrade attempt, 2026-09-24: WLED 16 hung on boot. Rolled back.**
OTA to 16.0.1 + usermod went through, but on every boot the controller joined
Wi-Fi (answered pings) and then stalled: no web server, LEDs dark, no Nibbles
heartbeat. The release build prints nothing, and GPIO 1 (serial TX) carries
LED channel 1, so there is no log. Recovered over the board's own USB port
(CH340; `/dev/cu.wchusbserial*`, auto-reset into the bootloader works):
- full flash image saved first (`wled/backup/.../flash_after_wled16.bin`);
- app0 still held the official 0.15.1 image byte for byte (WLED 16 was in
  app1), so erasing `otadata` (0xe000, 8 KB) made it boot 0.15.1 again;
- WLED 16 had rewritten `cfg.json` on first boot (kept the old one as
  `bkp.cfg.json`): `linked_remote` became a list, `hw.led.prl` (parallel I2S
  output) and some transition keys were dropped. The original `cfg.json` was
  uploaded back through `/upload` and the controller rebooted: 0.15.1, 1194
  LEDs, 42 presets, remote linked.
- Power the board from its LED supply while USB is connected: with the LEDs
  attached, USB power alone tripped the Mac's port over-current protection.
- DHCP gave it a new address afterwards (10.7.200.136).
- While the USB cable is connected, LED channel 3 (GPIO 3, the UART RX pin
  that the CH340 drives) goes dark; it comes back when USB is unplugged.
  (Since 2026-09-27 nothing is on channel 3, so USB can stay connected.)

**Cause, found on a bench ESP32 (same chip, the controller's config and
storage image, joined to the same Wi-Fi): the usermod.** Stock WLED 16 and the
build with the usermod disabled both ran fine. WLED starts QuickEspNow in
synchronous mode, where `quickEspNow.send()` busy-waits in WLED's main loop for
the "sent" callback. A send that fails, as the usermod's first heartbeats do
while Wi-Fi is still connecting in station mode, never gets one, so the loop
spun forever. (The dev node always ran as an access point, where sends
succeed, so it never showed.) Fixed in commit 581aaeb: the usermod sends with
`esp_now_send()` directly. On the bench it then boots, anchors channel 11 and
the leader eye locks on. `nibbles_shark_debug` sends WLED's debug log over UDP
to `NIBBLES_DEBUG_HOST` port 7868 for this kind of bench work.

**Upgraded 2026-09-25 (second attempt, with the fix): working.** After a
clean 15-minute bench soak, OTA to 16.0.1 + usermod (image from commit
581aaeb). It came back at 10.7.200.253 with 1194 LEDs at 49 fps,
AudioReactive running, anchoring channel 11 and hearing the leader eye.
Presets, LED outputs, WiZmote, mic pins and Wi-Fi all survived. The boot preset
(230) applies about 15 s after boot, but WLED then reports `ps: -1` instead of
230 (cosmetic). Pre-upgrade backup: `wled/backup/*-pre-retry/`.
After the upgrade the lights toggled on/off every 0.5-3 s (dimming and coming
back over the 0.7 s transition). Cause: the pushbutton configured on GPIO 0.
On the WT32-ETH01, GPIO 0 is the Ethernet chip's 50 MHz clock input, and
WLED 16 read the noise there as button presses (0.15 didn't). The button was
removed from the config (`hw.btn.ins` now empty; nothing on the board uses
it) and the toggling stopped. Don't put a button on GPIO 0 on this board.

**Power and the flash bump.** A flash bump turns every LED (2020-size
WS281x) full white. The LEDs run from a 100 W 12-24 V to 5 V converter (rated
20 A; an older 50 W one was replaced in 2025), fed from the pole base: 20 V from a USB-PD trigger board (3-5 A) or a
4S 21700 pack. The bench board handled flash, blackout and preset bumps fine.

*Measured 2026-10-02* on a bench supply at the pole base (in place of the
PD/battery input), full white at full brightness with the limiter off:
**about 100 W in**. After the converter's losses (~85-92%) that is about
16.5-18 A at 5 V, or **15.5-17 mA per LED at full white**, not WLED's
default estimate of 55 mA. That is 85-90% of the converter's 20 A, so it can
run full white, but it is more than a 60 W (20 V x 3 A) PD source can give.

*WLED's limiter now* (since 2026-10-02): every output estimates **17 mA per
LED** (`ledma`, the top of the measured range, so it errs safe) and the
global limit (`hw.led.maxpwr`) is **17000 mA**, now in real milliamps: 85%
of the converter's 20 A, so full white runs at about 95%. That needs a
supply that can give ~100 W into the converter (the bench supply, the 4S
pack); 9000 mA (full white at about half) was the setting before.

*Earlier brownouts*: with the old 55 mA estimate, "8000" (~2.5 A real)
browned out the controller on a flash and "5000" (~1.5 A real) survived.
The USB-PD trigger board is rated 5 A (100 W at 20 V), so the limit was the
PD source it was plugged into: on a V-mount battery with 100 W USB-PD
output the shark runs full white without trouble, as on the bench supply.
**Match the limit to the source**: 17000 mA for a 100 W PD source (with a
5 A cable), about 9000 mA for a 60 W one, lower for anything weaker (test
with a flash bump). Keep the limiter on, and retest
flashes whenever the power hardware changes. Backups from before these
changes: `wled/backup/20261002-0731-pre-ledma/`, `20261002-0736-pre-17000/`.

Upgrade plan (for later upgrades):
1. Back up its config and presets (WLED → Config → Security & Updates →
   Backup). LED and mic pins are in that config, not in the build.
2. Build `nibbles_shark`; the image is `.pio/build/nibbles_shark/firmware.bin`.
3. Upload it on WLED's Update page (OTA; GPIO 1 and 3 drive LEDs, so there is
   no USB serial). 0.15 and 16.0.1 use the same partition table
   (`tools/WLED_ESP32_4MB_1MB_FS.csv`; its filesystem shows 983 KB), so the
   saved config and presets stay. To roll back, upload the 0.15.1 image.
4. Check the Info page for "Nibbles radio", then on the base screen that WLED
   is heard and anchors the channel.

Known WLED issue to watch (fixed only in 17.0.0-dev): "ESP-NOW remote with no
Wi-Fi reboots every 15–20 min". Soak test 2026-09-24: the dev node (16.0.1 +
usermod, no Wi-Fi network configured so on its own AP, ESP-NOW anchoring and
receiving eye telemetry) ran 35 min with no reboot. Repeat on the real
controller after upgrading.
