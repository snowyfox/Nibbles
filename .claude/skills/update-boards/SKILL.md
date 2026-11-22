---
name: update-boards
description: Make a code change to any Nibbles board (eyes, base 1.91/3.49, Live/Standby monitors, brains, controller) and get it onto the hardware, by USB when the board is plugged in or over the air when it isn't. Use for any firmware edit, build, flash or update of the shark's boards, or a question about how to update one.
---

# Changing and updating the Nibbles boards

How a firmware change goes from code to the boards. The board facts (MACs,
roles) are in the root `CLAUDE.md`; each firmware's `CLAUDE.md` has the details.
Work in small verified steps, and let the user check anything visual.

## 1. Change, test, build

```sh
source ~/.espressif/tools/activate_idf_v5.5.5.sh
make -C shared/nibbles_link              # protocol host tests (after any change in shared/)
make -C firmware/eyes/test/host          # eye host tests
make -C firmware/base/test/host          # base host tests
idf.py -C firmware/eyes build            # -> firmware/eyes/build/nibbles.bin
idf.py -C firmware/monitor build         # -> firmware/monitor/build/nibbles_monitor.bin
cd firmware/base && idf.py -B build-191 -D SDKCONFIG=build-191/sdkconfig \
   -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.board_191" build   # 1.91 base -> build-191/nibbles_base.bin
idf.py -C firmware/base build            # 3.49 base (fallback) -> build/nibbles_base.bin
```

- A change in `shared/` (nibbles_link, nibbles_espnow, nibbles_ota) touches
  every firmware that uses it: rebuild all of them, and the WLED usermod if
  `nibbles_link.h` changed (`wled/README.md`, "Building").
- New messages go in `shared/nibbles_link` with a host test. Old boards ignore
  types they don't know, so keep `NL_VERSION` unless a struct changes.

## 2. Which board is where

Plugged in? `ls /dev/cu.usbmodem*`, then read each MAC **without leaving it in
the bootloader** (`esptool.py --port P read_mac`, never `--after no_reset`)
and match it to the root `CLAUDE.md` table. A brain's dev kit shows two ports
(USB-serial chip and native USB) with the same MAC. Close anything holding a
port first (`lsof /dev/cu.usbmodemNNN`; an old `tools/midi_keys/server.py`
held the base's once).

## 3a. By USB (board plugged in)

`idf.py -C firmware/X -p PORT flash` (the 1.91 base with the same `-B` and
`-D` as its build). Flash resets `otadata` to `ota_0`. **Never `app-flash`** on
these boards: it writes `ota_0` while the board may be running `ota_1`.
Eyes: flash only the **starboard** eye; it passes its build to the port eye
over the cable (about 18 s, within a minute of boot). The starboard eye's
build always wins.

## 3b. Over the air (nothing plugged in)

Needs a brain on (the Right Brain hosts the `Nibbles` access point on the
radio's channel, password `wled1234`) and the base on. No USB anywhere.

```sh
tools/nibbles_wifi.sh join                       # this Mac onto Nibbles (CoreWLAN)
python3 tools/base_ask.py "ota remote on eyes"   # or monitors; via the brain to the base
python3 tools/base_ask.py "ota remote"           # names, http://IP/, builds
python3 tools/base_ota.py wifi IP firmware/eyes/build/nibbles.bin
python3 tools/base_ask.py "ota remote off all"
# the base itself:
python3 tools/base_ask.py "ota wifi on"; python3 tools/base_ask.py "ota wifi"   # its address
python3 tools/base_ota.py wifi IP firmware/base/build-191/nibbles_base.bin
python3 tools/base_ask.py "ota wifi off"
tools/nibbles_wifi.sh leave                      # the Mac back on its own network
```

- **No internet while on Nibbles** (Wi-Fi only, e.g. a hotel): Claude can't act
  while the Mac is offline, so start the unattended push and read its log after:
  `python3 tools/ota_push.py --detach eyes monitors base` (any of the three).
  It joins Nibbles, updates each group, checks each board kept its image
  (the eyes: both eyes), sends everything off Wi-Fi, rejoins the usual
  network, and ends the log (`logs/ota_push_*.log`) with `RESULT`. Boards
  already on the build are skipped. Tested 2026-11-17: all five boards in 4 min.
  Wait for it with a background loop on the log, not by polling.
- **Set the Mac's Wi-Fi DNS to a manual server (e.g. 9.9.9.9) first** (the user does
  it: System Settings, Wi-Fi, Details, DNS; it's for the Wi-Fi service, not per network).
  WLED's access point answers every name lookup with 4.3.2.1, so on joining, every
  background service on the Mac opened connections to the brain and its web server
  stopped answering (2026-11-21, a hotel: three failed pushes, then fine with 9.9.9.9).
  Leave the captive-portal window alone (closing it can drop the network); the push
  waits 30 s after joining for it.
- **One group at a time.** The brain's access point takes only a few clients;
  with the eyes, both monitors and the Mac on it the Mac couldn't join
  (`tmpErr`). Eyes, then monitors, then the base.
- A request can go unanswered while the base or a board is joining (it is off
  the radio's channel for a moment): ask again.
- Boards leave Wi-Fi on their own after 15 min (or `ota remote on X MIN`),
  later if an update is being written; a restart also ends it.
- The base turns its Bluetooth (clock from a phone) off when its Wi-Fi goes on;
  with both on, it joined but answered nothing.
- On the base's USB console the same commands work, plus `wifi set SSID PASS`
  (the network it sends the boards). The keypad also turns the base's own Wi-Fi
  on: PAGE held 5 s with the keys locked.
- **Brains and the controller** run WLED: upload `.pio/build/<env>/firmware.bin`
  on their WLED Update page (`http://IP/update`, or
  `curl -F "update=@firmware.bin" http://IP/update`) from a computer on
  `Nibbles`. The controller keeps its radio off: have it join `Nibbles` with
  a brain's `{"nibbles":{"ctlwifi":true}}` (POST to the brain's `/json/state`)
  or the button on a brain's Info page; it stays on until it restarts. (The
  base's `wled wifi on` does the same but only from its USB console.) Back up
  config and presets first. See
  `wled/README.md` (the release name must match, "ESP32_Ethernet" on the
  controller; `ota.same-subnet` off).

## 4. Check it

- Every ESP-IDF board keeps a new image only after 30 s (the eyes also need to
  have drawn a frame in the last second), otherwise it goes back to the old one.
  Over USB: `ota status` (base, monitors) shows the slot, build and "on trial".
  Over the air: `ota remote` / `ota wifi` show the build, which is the first 16
  hex digits of the ELF's SHA-256 (`base_ota.py` prints the one it sends).
- Read the board's status log (once a second on the eyes and base, every 5 s on
  the monitors). The eyes need their screens fitted for real frame rates (no TE
  without a panel: ~12 fps).
- Ask the user to confirm anything visual or physical.

## 5. Record and commit

Update the firmware's `CLAUDE.md` (and the root one for new commands or
boards). Commit only when asked; push every commit to `origin` (`master`).
