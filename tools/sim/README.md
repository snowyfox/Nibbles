# Nibbles lighting simulator

Plays sound (the computer's microphone or an audio file) through WLED
AudioReactive's own processing and shows the shark's LEDs reacting, driven by
a real WLED running the shark's config and presets ("route A").

```
python3 tools/sim/server.py          # then open http://localhost:8080/
```

## Pieces

- **Simulator WLED**: the spare classic ESP32 bench board (CP2102,
  MAC d4:8a:fc:c5:d9:10, powered over USB) running the same WLED 16 +
  Nibbles build as the shark, with the shark's config and presets, except:
  - LED output is one DDP network bus of all the LEDs to this computer
    (UDP 4048), so every LED comes back at full resolution;
  - AudioReactive is in receive mode (UDP 11988);
  - the Nibbles usermod is off (so it doesn't compete with the real
    controller on the radio), name "Nibbles sim", mDNS `wled-sim`.
  Build its storage image from the controller's current files as in
  `wled/backup/sim/` (not committed); its address is found by MAC.
- **`server.py`** (Python standard library only): serves `web/`, relays the
  page's audio sync packets to WLED (multicast 239.0.0.1:11988 and direct to
  `--wled`), and turns WLED's DDP frames into WebSocket messages for the page.
- **`src/ar_sim.c`**: WLED AudioReactive's sender-side processing (FFT, 16
  bands, AGC, peaks) ported to C from WLED 16.0.1 (EUPL-1.2), with the
  shark's settings (squelch 10, gain 30, AGC vivid). `test/ar_host.c` checks it
  on the host with a synthetic track.
- **`web/`**: the page. `sim.js`/`sim.wasm` are built by `build.sh` with
  Emscripten (`source <emsdk>/emsdk_env.sh` first); they are committed so the
  page runs without it.
- **`web/layout.json`** (optional): LED positions, `{"name": ..., "leds":
  [[x, y, z], ...]}` in WLED order. Without it the page draws one row per
  output channel.

## 3D view
The 3D view (three.js, WebGL) shows the shark's body from the Fusion export
with every LED at its real position. Drag to orbit and scroll to zoom. On a
Mac without working WebGL (e.g. macOS on an unsupported Intel Mac), the page
switches to a simpler 3D view drawn without WebGL: LEDs, tubes and pole, no
body. For the full view there, `./open_chrome.sh` opens the simulator in a
separate Chrome window that uses Chrome's software GPU (SwiftShader).

## Eyes
The eyes card runs the eye firmware's own code (`firmware/eyes/main`:
audio_analysis, eye, presets, render_core, plus `nl_ar` from the shared
protocol), built into `sim.wasm` by `build.sh` and driven by
`src/eyes_wasm.c` the way `main.c` drives it. The starboard eye leads
(preset cycling, blinks, bumps) and the port eye follows its shared state, as
over the eye cable. "Hears" picks the audio: WLED's AudioReactive analysis
converted by `nl_ar` (what the shark's eyes use with `EYES_AUDIO_FROM_WLED`),
or the port eye's own analysis of the same sound resampled to 16 kHz. The
simulated eyes don't move, so there is no look, twist or dance input.
The 3D views (and the side view) also show each eye's picture on its screen at
the eye opening; **Head** zooms in on them. Where the screens sit is set in
`make_layout.py` (`eye_screen_radius_mm`, `eye_screen_x_mm`). Rebuild
after changing the eye code: `./build.sh` (Emscripten).

## Base station
The **BASE STATION** card is the base's keypad and screen. The keys run
through the base firmware's own Live / Standby logic
(`firmware/base/main/controls.c`): `server.py` builds it with the computer's
C compiler into `build/libbase.so` (rebuilt when the source changes) and calls
it through ctypes, so it needs no Emscripten. The page draws the screen like
the base's main page (`display.c`) and carries out what the logic asks:
- body (WLED) presets and brightness go to the WLED card's address over its
  JSON API, as the WLED card's own controls do (the bench simulator, or the
  live shark); fin following too (`{"nibbles":{"finfollow":..}}` on the
  shark; the bench has no usermod, so its fins just show locked);
- the eyes' presets and bumps go to the simulated eyes;
- WLED bumps are imitated over JSON (flash: white at full brightness;
  blackout: off; preset: that preset; then back), since on the shark they
  go by radio.

Click (and hold) the keys, or use the computer's keyboard (letters by the
character they type, so Dvorak and other layouts work): 1-8, A all,
B body, E eyes, F fins, P page, Shift or L for LATCH, Space TAKE, X flash,
Z blackout, S fin sensor, U unlock (hold 1 s). Or the number pad: 1-8 the
numbers, 0 TAKE, Enter LATCH, / body, * eyes, - all, = (or Clear) fins,
+ page, . flash, 9 blackout. Numbers and TAKE are bumps;
tap LATCH while holding one to keep it. The STANDBY panel works as TAKE by touch.
POWER is simulated: the pack slider, "On the dock", and the lights' power
estimated from the LED frames (17 mA per LED at full white, a 90% converter).

## Driving the real base
**Keys drive: the real base, on USB** (base card) sends the page's keys
(clicks, the computer keyboard, the number pad) to the base prototype on
USB instead of the simulated one:
- `server.py` writes `key NAME down|up` to its serial console, so the real
  base runs the keys and sends real commands and bumps over the radio.
- The card shows the base's own screen (a screenshot about once a second)
  and its key LED colours (`leds on`).
- The port is found by the base's USB serial number (`BASE_SERIAL`), or
  give it with `server.py --base /dev/cu.usbmodemNNN`.
- Opening the port can restart the base: give it ~15 s to find the radio
  channel again.
- The card shows the base's screen as run-length encoded screenshots
  (`shot rle`, a few KB each): one a second and one just after each key.
  The base prints them from their own task, so keys never wait behind them.
- With the main board as the brain, start the server with
  `--source shark --shark 4.3.2.2 --wled 4.3.2.2`: it mirrors the brain from
  the start (the page's choice of source is lost when the server restarts).

## Fin presets and eye settings
- **Fin preset** (WLED card) lists the WLED's fin presets (80 = follow the
  body, 81-96; see `wled/README.md`, "Fin presets") and applies one. The
  label says which projection is in use (side or top view), from the
  usermod's mirror on a live WLED, else from the preset. The fins only show
  their own effect on a WLED with the new map and the usermod running.
- **Eye settings in presets:** a body preset picked on the WLED card, or
  applied from the base card with ALL + number, sends its
  `"nibbles":{"eyes":{...}}` to the simulated eyes, as the usermod does on
  the shark. That covers the eyes' own preset, reactivity and auto-cycling.
  Not while the eyes follow the live shark: its usermod sends them to the
  real eyes.
- **Whole looks** (`"eyes":{"look":{...}}`) go into the simulated eyes'
  look slots, as on the real eyes: blinked to, in the slot not on screen,
  with spirals snapped to the nearest built-in spiral shape.
- **Building `sim.wasm`:** `src/eyes_wasm.c` (`eyes_look()`) is built in with
  `./build.sh`. Emscripten 6.0.13 was used (2026-10-28); install it with
  `tools/sim/build/get_emsdk.sh`, then
  `source <emsdk>/emsdk_env.sh`.

## The live shark
**Lights: the shark, live** (WLED card) shows exactly what the shark's own
controller shows, instead of the bench simulator. `server.py` asks the
controller's Nibbles usermod to stream its LEDs (see `wled/README.md`,
"Mirrors the lights to the simulator") and passes them to the page; the
controller listens with its own mic, so the page's sound isn't used for the
lights. The eyes switch to **Hears: the shark, live**: they run on the audio
features the controller sends the real eyes and follow the real leader eye's
preset and reactivity (their blinks and glances are their own). As on the
shark, the eyes' brightness always follows the shown WLED's master brightness.
In this mode the preset and brightness controls change the real shark.
The controller's address comes from `server.py --shark` (default
10.7.200.253); the controller needs a usermod build with the mirror.

## 2D map for WLED
`make_ledmap.py` builds `wled/ledmap/ledmap.json`, a WLED 2D matrix of the
shark from this layout (see `wled/README.md`, "2D map"). With the map on the
bench WLED, the simulator shows its 2D effects on the shark.

## Fins
Each fin swings about its hinge (the FinMount axis from the Fusion model,
written into `layout.json` as `fin_hinges` by `make_layout.py`), from straight
up (+90°) to straight down (−90°); the model has them at about −34°. The
**Port fin** / **Starboard fin** sliders (WLED card) turn a fin's LEDs, its
tube outline and, in the WebGL view, its part of the shark mesh. In live mode
a fin with a sensor follows the real one (the fin sensor node,
`firmware/fins`, via the controller's mirror) and its slider shows "(live)".
**Port fin sweep** / **Starboard fin sweep** (swings a minute, 0 = off) swing
a fin through its whole range, -90..+90 deg. With the live shark the
controller does the sweep (`{"nibbles":{"finsweep":{...}}}`, renewed every
2 s), so the real fin LEDs follow it as if the fin moved, and the page shows
the controller's angle "(sweep)"; otherwise the page animates the fin.
When fin following is off on the shark (the base's FINS switch), both fins
show "(locked)" at the modelled angle.

## Spin
**Spin about the pole** (WLED card, 0 to 60 turns a minute, i.e. up to once a
second) turns the whole simulated shark (LEDs, body, eyes, fins) about the
pole's axis, counter-clockwise seen from above, in every view. The axis
indicator turns with it (it shows the shark's own axes), and LEDs seen
through the body are still dimmed correctly. The flat views keep a steady
scale while it turns. It only moves the picture: nothing is sent to the shark.
