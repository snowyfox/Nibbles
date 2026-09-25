# Nibbles system architecture

How the boards in the shark work together, and the order they're being built
in. Agreed 2026-09-24; update this file as decisions change.

## Pieces
- **Two eyes**: Waveshare ESP32-S3-Touch-AMOLED-1.75, mounted back to back
  120 mm forward of the pole, screens facing out to each side. They are linked
  by a cable between their 8-pin headers.
- **WLED controller**: an existing ESP32 running WLED, which drives all the
  other lights and has an external digital mic. It joins a phone hotspot when
  testing (at home or in a hotel) and falls back to its own AP at a festival,
  so **its Wi-Fi channel varies**.
- **Base station** (prototype on an ESP32-S3-Touch-LCD-3.49): an ESP32 in the pole base with battery
  management, status screen(s), and buttons to change eye and WLED presets;
  later, DMX-style bump buttons for momentary effects.

## Repository
One monorepo: a single shared protocol, used by every firmware, changes in one
commit with all its users. WLED is **not forked**: WLED 16.x builds usermods
as PlatformIO libraries through `custom_usermods`, and they can live outside
its tree (`symlink:///path`). A stock WLED checkout pinned to a release tag,
plus the `platformio_override.ini` from this repo, builds WLED with our usermod.

```
CLAUDE.md                 system overview and conventions
docs/                     this file; later protocol.md, wiring.md
shared/nibbles_link/      pure-C protocol, framing and CRC; ESP-IDF component and
                          PlatformIO library; host tests            (Phase 1)
firmware/eyes/            eye firmware (ESP-IDF)
firmware/base/            base station firmware (ESP-IDF)           (Phase 4)
wled/usermod_nibbles/     WLED usermod                              (Phase 3)
wled/platformio_override.ini                                        (Phase 3)
tools/                    capture, recording and replay helpers
```

## Roles
- **Starboard eye = leader.** It is the eyes' radio gateway (ESP-NOW) and runs
  the shared "brain": preset choice and timing, swap and random blinks,
  sleep/wake, hype, colour drift and tempo.
- **Port eye = ears.** It runs the mic and the audio analysis and sends the
  results to the leader. It does no radio work.
- Each eye renders its own screen and uses **its own IMU** for pupil look and
  twist (the mounts are mirror images). Roles come from the board's MAC, as the
  side does today.
- If the link is silent for more than 250 ms (`LINK_STATE_MAX_AGE_MS`), each
  eye runs standalone, so a broken cable never blanks an eye.

Spreading the jobs this way keeps each eye's frame budget: the render path
has only ~3–7 ms of slack per frame.

## Eye link (wired)
UART1 at 1 Mbaud on header GPIO 17 (TX) and GPIO 18 (RX), crossed between the
eyes, plus GND. That leaves GPIO 16, 43 and 44 free (UART0 is unused because
the console is on USB-JTAG), exactly enough for an optional I2S mic on one
eye. Framing: COBS with CRC-16, a message type and a sequence number.

| Direction | Rate | Content |
|---|---|---|
| port → starboard | ~31 Hz | audio features: level, average, loudness, warmth, beat count, beat period, rhythm, noise floor, gain (~32 B) |
| starboard → port | every frame | shared eye state: state, lid, blink/swap phase, swap + preset index, hype, hue, intensity, wobble, ring phase, tempo, ripples (~40 B) |
| both ways | 2 Hz | heartbeat: role, firmware version, fps, late frames, temperature |

## Radio (ESP-NOW)
- **WLED is the channel anchor.** Its usermod broadcasts a Nibbles heartbeat
  on whatever channel WLED is on. The leader eye and the base scan channels
  until they hear it, lock on, and rescan after ~3 s of silence. The same code
  works at home (hotspot) and at a festival (WLED's AP).
- **Fallback anchor**: if the base hears no anchor for 12 s (WLED off, or
  running without the usermod), it anchors channel 6 itself. It then checks
  one other channel for 300 ms every 3 s, so it finds WLED within about 40 s
  and hands over to it.
- State that can be sent again safely (heartbeats, telemetry, audio features)
  is broadcast at a fixed rate.
- Commands are unicast to known peers with an ack and retries.
- Momentary "bump" actions send start, then a keepalive every 100 ms, then
  stop. The receiver releases automatically after 300 ms without keepalives,
  so a lost stop can't leave an effect stuck on.
- Every message carries a network ID, protocol version and sequence number.
  ESP-NOW encryption can be added later.
- Messages (`shared/nibbles_link/include/nibbles_link.h`, protocol version 3):

  | Message | From → to | When |
  |---|---|---|
  | `HEARTBEAT` | everyone → broadcast | 2 Hz; anchors 4 Hz with `NL_HB_ANCHOR` and their channel |
  | `EYE_TELEMETRY` | leader eye → broadcast | 2 Hz: preset and name, state, link, both fps, bpm, brightness, auto-cycle |
  | `WLED_TELEMETRY` | WLED → broadcast | 2 Hz: on, brightness, preset and name, effect, palette, fps, LEDs |
  | `CMD` / `ACK` | base → eyes or WLED, unicast | on demand: preset set/next/prev, brightness set/step, eyes' auto-cycle; up to 3 tries of 60 ms |
  | `BUMP` | base → broadcast (target mask) | START, HOLD every 100 ms, STOP: flash, blackout, preset while held |
  | `AUDIO` | WLED → broadcast | every 32 ms when its `audio` setting is on |
- The WLED usermod receives messages through
  `Usermod::onEspNowMessage(sender, payload, len)`, so WLED's own ESP-NOW
  handling is untouched.

## Audio source ("the system's ears")
A single "audio features" message, whatever the source. Later, the WLED usermod
can publish WLED AudioReactive's analysis of its external mic (through AR's
`um_data`), and the eyes can use that instead of their own mics if it detects
beats better. Raw audio over ESP-NOW isn't worth it.

Built (not yet tried on real music): the usermod's `audio` setting broadcasts
`nl_audio_t` every 32 ms from AudioReactive's smoothed volume, FFT bands and
beat peaks (`nl_ar_update`: warmth from bass vs treble bands, tempo from the
median peak interval, folded to 100–200 bpm like the eyes'). The leader eye
uses it instead of the eyes' mics when built with `EYES_AUDIO_FROM_WLED 1`,
falling back to the port eye, then its own mic, when it stops.

## Roadmap
| Phase | Work | Status |
|---|---|---|
| 0 | Restructure into this layout | done |
| 1 | Eye-to-eye link: protocol + framing with host tests; UART link task; leader/ears roles; shared-state vs local-state split; standalone fallback; link stats in the status log | done: both boards linked (0 CRC errors, 0 late frames); visual sync confirmed; unplugging either data wire falls back to standalone at once and re-links on reconnect |
| 2 | ESP-NOW on the leader eye: channel scan and lock, heartbeat, telemetry; measure Wi-Fi's cost to the frame rate (keep Wi-Fi on core 0) | done: `shared/nibbles_espnow`; leader eye locks onto the anchor, sends telemetry, takes preset commands (acked in ~2 ms); 0 late frames with Wi-Fi on. Base prototype (`firmware/base` on an ESP32-S3-Touch-LCD-3.49) anchors channel 6 until the WLED usermod exists |
| 3 | WLED 16.x upgrade (back up config and presets first) + usermod: channel-anchor heartbeat, preset commands, bump effects, optional AudioReactive features. Watch for: "ESP-NOW remote with no Wi-Fi reboots every 15–20 min" (fixed only in 17.0.0-dev) | in progress: `wled/usermod_nibbles` builds into WLED 16.0.1 (see `wled/README.md`); on a 3.49 dev node it anchors the channel, the eyes lock onto it and their telemetry reaches WLED. Scanners now jump to the anchor's advertised channel (channels overlap). WLED preset and brightness commands verified from the base (acked in 3–15 ms; next/previous wrap over existing presets; a missing preset is refused; preset names reach the base screen; commands during a bump apply on release). The base hands the channel over to WLED's anchor. Real controller not upgraded; AudioReactive features not started |
| 4 | Base station: choose hardware (display, battery chemistry/BMS, how battery state is read, buttons); firmware for status screens and preset buttons | prototype firmware on a 3.49: scans for WLED, logs eye and WLED telemetry, eye and WLED preset commands from buttons and USB serial, LVGL status screen (eyes, WLED, radio, events). Hardware choices (battery, BMS, buttons) not made |
| 5 | Bump / DMX-style control: momentary effects, preset while held, flashes; under 20 ms from button to light | protocol (`nl_bump.c`, host tested) and WLED side done: flash, blackout and preset-while-held with state restore, verified from the base. Eyes: flash (startle: full glow, small pupil, ripple), blackout (lids shut, master level 0, outline included) and preset while held, verified on both linked eyes. Latency (leader eye, radio arrival → frame sent; the log prints it per bump): 35–68 ms, median 54 ms over 12 flashes; the radio adds ~1–2 ms (command round trips are 2–4 ms). The eyes can't meet 20 ms: a frame is 34 ms at 29.5 fps and takes ~26 ms to send over QSPI, and the eye task takes input once per frame. WLED not measured yet |

## Open decisions
- Base station hardware and battery system (Phase 4).
- WLED controller board type (sets the PlatformIO env) and LED count (Phase 3).
- Whether the eyes switch to WLED's mic features (after Phase 3, compared on
  real music).
- Eye cable: GND, TX and RX. Power stays separate unless the boards share a
  supply.
