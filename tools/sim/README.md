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
  - LED output is one DDP network bus of all 1194 LEDs to this computer
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
