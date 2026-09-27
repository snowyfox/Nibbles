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
  later, DMX-style bump buttons for momentary effects. Its power system
  (internal pack, power board, battery dock) is designed in
  [power.md](power.md).

## Repository
One monorepo: a single shared protocol, used by every firmware, changes in one
commit with all its users. WLED is **not forked**: WLED 16.x builds usermods
as PlatformIO libraries through `custom_usermods`, and they can live outside
its tree (`symlink:///path`). A stock WLED checkout pinned to a release tag,
plus the `platformio_override.ini` from this repo, builds WLED with our usermod.

```
CLAUDE.md                 system overview and conventions
docs/                     this file; power.md (power system design); later protocol.md, wiring.md
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
falling back to the port eye, then its own mic, when it stops. First live run (2026-09-25):
the leader switched to "audio from WLED" with 0 late frames, but
AudioReactive's peak flag fired about 9 times a second. The converter now
counts at most one beat per 250 ms, and needs 75% of intervals within 8% of
the median before it reports a tempo. Loudness reads 0.6-0.8 after
AudioReactive's gain control; still to be compared on real music. The raw
peaks looked great on the eyes, so `nl_audio_t` (protocol version 4) carries
both `beat_count` (steady beats) and `peak_count` (every peak), and each eye
preset picks which one it reacts to (`peak_beats`).

## Roadmap
| Phase | Work | Status |
|---|---|---|
| 0 | Restructure into this layout | done |
| 1 | Eye-to-eye link: protocol + framing with host tests; UART link task; leader/ears roles; shared-state vs local-state split; standalone fallback; link stats in the status log | done: both boards linked (0 CRC errors, 0 late frames); visual sync confirmed; unplugging either data wire falls back to standalone at once and re-links on reconnect |
| 2 | ESP-NOW on the leader eye: channel scan and lock, heartbeat, telemetry; measure Wi-Fi's cost to the frame rate (keep Wi-Fi on core 0) | done: `shared/nibbles_espnow`; leader eye locks onto the anchor, sends telemetry, takes preset commands (acked in ~2 ms); 0 late frames with Wi-Fi on. Base prototype (`firmware/base` on an ESP32-S3-Touch-LCD-3.49) anchors channel 6 until the WLED usermod exists |
| 3 | WLED 16.x upgrade (back up config and presets first) + usermod: channel-anchor heartbeat, preset commands, bump effects, optional AudioReactive features. Watch for: "ESP-NOW remote with no Wi-Fi reboots every 15–20 min" (fixed only in 17.0.0-dev) | in progress: `wled/usermod_nibbles` builds into WLED 16.0.1 (see `wled/README.md`); on a 3.49 dev node it anchors the channel, the eyes lock onto it and their telemetry reaches WLED. Scanners now jump to the anchor's advertised channel (channels overlap). WLED preset and brightness commands verified from the base (acked in 3–15 ms; next/previous wrap over existing presets; a missing preset is refused; preset names reach the base screen; commands during a bump apply on release). The base hands the channel over to WLED's anchor. Real controller not upgraded; AudioReactive features not started |
| 4 | Base station: choose hardware (display, battery chemistry/BMS, how battery state is read, buttons); firmware for status screens and preset buttons | prototype firmware on a 3.49: scans for WLED, logs eye and WLED telemetry, eye and WLED preset commands from buttons and USB serial, LVGL status screen (eyes, WLED, radio, events). Power system designed in [power.md](power.md), not built; BMS/gauge and buttons still open |
| 5 | Bump / DMX-style control: momentary effects, preset while held, flashes; under 20 ms from button to light | protocol (`nl_bump.c`, host tested) and WLED side done: flash, blackout and preset-while-held with state restore, verified from the base. Eyes: flash (startle: full glow, small pupil, ripple), blackout (lids shut, master level 0, outline included) and preset while held, verified on both linked eyes. Latency (leader eye, radio arrival → frame sent; the log prints it per bump): 35–68 ms, median 54 ms over 12 flashes; the radio adds ~1–2 ms (command round trips are 2–4 ms). The eyes can't meet 20 ms: a frame is 34 ms at 29.5 fps and takes ~26 ms to send over QSPI, and the eye task takes input once per frame. WLED not measured yet |

## Open decisions
- Base station hardware and battery system (Phase 4): see [power.md](power.md)
  and its own open decisions.
- WLED controller board type (sets the PlatformIO env) and LED count (Phase 3).
- Whether the eyes switch to WLED's mic features (after Phase 3, compared on
  real music).
- Eye cable: GND, TX and RX. Power stays separate unless the boards share a
  supply.

## Top-of-pole wiring (planned 2026-09-27)
The WLED controller sits in a weatherproof box on top of the pole; the eyes
are sealed in small weatherproof domes on the shark, with no access to their
boards or ports once installed. Plan for programming and powering them:
- **USB hub in the controller box**, powered from the box's 5 V (not from
  the computer). Its upstream USB-C socket sits inside the box (the box is
  weatherproof and opens easily, so the socket needs no sealing of its own);
  add 5.1 kOhm from CC1 and CC2 to GND if the hub board lacks them, and a
  small USB TVS array. One cable from a computer then reaches every board:
  - **each eye**: a 4-wire harness (5 V, GND, D+, D-; under 1 m; twist the
    data pair) ending in a USB-C plug in the eye's own port, inside its
    dome (no cable gland: the plug and the harness end are sealed in the
    dome). Flashing and the once-a-second status logs work as over a
    normal USB cable, and even a crashing build can be re-flashed (the
    ESP32-S3's USB programming is in hardware). The eyes are told apart by
    their serial numbers (MACs);
  - **the WLED controller's own USB port** (CH340), permanently: nothing is
    on its channel 3 any more (see the WLED outputs in `wled/README.md`), so
    the USB chip driving GPIO 3 no longer blanks LEDs. Opening its serial
    port may reset WLED; only done deliberately at the bench;
  - a spare port for later (the base station, a fin-sensor board).
- **Eye power**: a separate 5 V feed from the box's supply, not from the LED
  distribution (full-white flashes sag that rail; it browned out the
  controller before), with a few hundred uF at the eyes or the hub outputs.
  Both eyes are loads on one supply here; the old "never link VBUS" rule was
  about two eyes each on their own USB supply being joined, so never also
  plug an eye into a separate supply while it is wired in.
- **Hub hardware**: start with an off-the-shelf FE1.1s or CH334/CH335 USB 2.0
  hub module (a few dollars; one whose downstream ports take their 5 V from
  a supply input you feed). Fold it into a custom box PCB later (hub, USB-C
  with ESD, locking harness connectors, per-eye fuse and bulk capacitors),
  alongside the planned custom base boards.
- Over-the-air updates for the eyes (below) become optional with this.

## Ideas for later
- **Over-the-air updates for the eyes** (2026-09-26; not started). Today each
  eye has one 8 MB app partition (`firmware/eyes/partitions.csv`) and no
  update code, so every update needs USB; the base is the same. Plan:
  1. One last USB flash per eye to a partition table with `otadata` and two
     OTA app slots (16 MB flash, the image is ~0.8 MB, so e.g. 2 x 4 MB);
     NVS stays where it is.
  2. Leader eye: only when asked (a base command or a button), join the same
     Wi-Fi as WLED (hotspot or its AP; same channel as ESP-NOW, so the radio
     keeps working) and take an upload from the Mac over HTTP, like WLED's
     update page. Off the network otherwise, so the frame rate is untouched;
     measure it while connected.
  3. Port eye: the leader passes the image on over the eye cable (1 Mbaud,
     roughly 10-20 s), so the port eye needs no radio.
  4. Rollback: a new image is marked good only after it boots and renders
     normally; otherwise the bootloader goes back to the previous one, so a
     bad update can't leave an eye dead on the pole.
  The base could get the same later.
- **Fins: independent effects and following the real fin angle** (2026-09-26;
  not started). The shark's WLED is a 2D matrix (side view, `wled/README.md`
  "2D map"); the usermod already copies partner LEDs each frame.
  - *Fins on their own matrix for some presets*: grow the grid (70 x 35 side
    view plus fin panels below it, each fin face-on, ~20 x 12 cells, and
    optionally a one-row strip per fin, 115 cells, for 1D effects along a
    fin). The fin LEDs live in the fin panels; their spots in the side view
    become empty cells. Every frame the usermod picks each fin's source from
    which segments the preset has switched on: only the side view on ->
    fins take the side view's colour at their spot (as now); a fin panel or
    strip segment on -> the fins show that effect; port and starboard can be
    separate segments or copy each other. Plain WLED presets, no special flag.
    Cost ~7 KB more RAM (~3,100 cells). The body could get the same one-row
    treatment (channel 1 as two rows of ~209; rows max 255) so 1D-style
    presets work in 2D.
  - *Fins that follow their real position*: each fin hangs on one hinge (the
    FinMount, axis along the fin root, tilted ~44 degrees nose-up), so one angle per
    fin says where it is. Sense it with a magnetic angle sensor on the hinge
    (AS5600 + magnet; absolute, no drift, analog out) rather than an IMU (an
    IMU needs the body's own motion subtracted, e.g. from the eyes' IMUs). A
    small ESP32-C3 near the fins, powered from the fin LEDs' 5 V, broadcasts
    the angles over ESP-NOW (a new "fin pose" message in the shared
    protocol) at 50-100 Hz; the Bong69's spare outputs are probably
    output-only buffers, so wiring into the controller is unlikely (unchecked).
    The usermod rotates each fin LED's rest position about the hinge axis by
    the angle, projects it onto the side view and samples the colour there
    (~0.2 ms/frame, ~3 KB for the rest positions, written into the map file
    by `make_ledmap.py` from the Fusion geometry). In fin-panel mode the angle
    could drive the fin effect instead (e.g. flap strength -> brightness).
  - Order: simulator first (per-fin hinge sliders; the 3D view rotates the
    fins), then the usermod on the bench fed a test angle through JSON, then
    hardware. Unknowns: how far the fins swing, room at the hinge for a
    magnet and sensor, whether a wire can reach the fins.
