# Nibbles

Firmware for the eye of a festival totem: a shark on top of a 3 m pole that
looks like it swims in the sky and dances to the music. The eye is a round
AMOLED showing an animated neon eye that reacts to the music (built-in mic)
and to the pole's motion (IMU). Target music: trance, uplifting melodic trance,
EDM, dubstep, drum and bass; tempos up to 180+ bpm.

## Hardware

Waveshare **ESP32-S3-Touch-AMOLED-1.75**:
- ESP32-S3R8 (bare chip, no module), 16 MB flash, 8 MB **octal** PSRAM.
- 466×466 round AMOLED, CO5300 driver over QSPI (GPIO 4-7 data, 38 clk, 12 CS,
  39 reset, **13 TE**). Panel refreshes at ~59 Hz on its own.
- QMI8658 IMU (I2C 0x6B), ES7210 dual mic ADC (0x40), ES8311 codec (0x18),
  AXP2101, TCA9554, PCF85063, CST9217 touch (unused: the eye is 3 m up).
- I2C on GPIO 14/15, shared by all chips. BOOT button on GPIO 0.
- Pin reference: https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/blob/main/HARDWARE_REFERENCE.md

Two identical boards are mounted **back to back** as the shark's eyes, 120 mm
**forward** of the pole's twist axis (toward the nose), screens facing out to
each side: **starboard** = 28:84:85:3A:DA:78 (the original), **port** =
28:84:85:3B:6F:F4. One firmware binary serves both; the port board is picked
out by MAC (`PORT_EYE_MACS` in `config.h`, logged as `motion: port eye` at
boot). The starboard screen's +x points toward the nose, the port screen's
toward the tail. The starboard board is rolled ~30° in the screen plane.
The IMU's +X axis points toward the bottom of the screen (`EYE_MOUNT_ROTATION 90`).

## Build, flash, test

ESP-IDF **v5.5.5**, installed with Espressif's installation manager. `export.sh`
does not work here; use the EIM activation script:

```sh
source ~/.espressif/tools/activate_idf_v5.5.5.sh
idf.py build
idf.py -p /dev/cu.usbmodemNNN flash      # port varies (201, 401 seen): ls /dev/cu.usbmodem*
```

Host tests (pure-C analysis, eye behaviour and presets; no hardware needed):

```sh
make -C test/host            # 143 checks, prints ok/FAIL, non-zero exit on failure
```

Reading the board: open the port with pyserial from the activated IDF env and
toggle RTS to reset (the USB-JTAG port can be held by another serial monitor;
check with `lsof`). Once a second the firmware logs one status line: fps,
per-frame setup (`prep`) and send time, late frames, sound level / noise floor /
mic gain, loudness, warmth, beats, tempo, rhythm strength, look offset, twist
rate and dominance, jolt, dance score and period, motion activity, eye state,
lid and hype. Preset changes are logged too.

Workflow that has worked well: change code → `make -C test/host` → `idf.py
build` → flash → read the status log. The user checks what the screen looks
like; ask them to confirm anything visual.

Commits: the user's git config GPG-signs by default; they have asked for
**unsigned commits** (`git commit --no-gpg-sign`). Only commit when asked.

## Layout

```
main/
  config.h            all tuning constants (thresholds, timings, gains, geometry)
  main.c              boot, eye task (core 1): sensors -> eye -> render, preset cycle, BOOT button, status log
  audio.c             mic capture task; applies the mic gain the analysis asks for
  audio_analysis.c/h  pure C: level, AGC, noise floor, loudness, warmth, onsets, beats, tempo
  motion.c            IMU task (200 Hz)
  motion_analysis.c/h pure C: gravity, pupil spring, twist, dance, motion activity
  eye.c/h             pure C: behaviour (sleep/wake, blinks, swap blink, hype, colour drift, ripples)
  presets.c/h         18 visual presets (data) + tempo-locked spin rate
  render.c/h          custom renderer straight to the CO5300 (no LVGL)
  sensors.h           audio/motion task interfaces
components/bsp_extra/ copied from Waveshare's 05_Spec_Analyzer (Apache-2.0), plus a mic gain setter
test/host/            host test harness and recording replay tools
```

Managed components: `waveshare/esp32_s3_touch_amoled_1_75` (BSP), `waveshare/qmi8658`.

## How it works, and why

### Rendering (`render.c`)
- Drives the panel directly via `bsp_display_new()` (LVGL is never started),
  in 466×16 RGB565 strips, 3 DMA buffers, both cores (helper task on core 0 at
  priority 7 renders the bottom half of each strip).
- Every pixel is looked up from precomputed maps in PSRAM, not computed:
  a **pupil distance map** (608×608, covers the look range, so moving the pupil
  just offsets the read), an **outline map**, and per spiral preset a **spiral
  map** (distance bin << 6 | spiral phase). Per frame only small colour tables
  are rebuilt (`prepare()`).
- Why: the S3 FPU has no sqrt instruction (`sqrtf` is a slow software call),
  `expf` is software too, and the CPU does about one instruction per cycle.
  The first per-pixel maths version ran at 10.7 fps. Glow rings use a gaussian
  lookup table instead of `expf`; the spiral table is built with integer maths.
- **Tearing fix**: each frame starts on the TE falling edge (GPIO 13; TE is
  high ~0.6 ms of blanking). A frame takes ~26 ms to send, between one and two
  refreshes, so the first refresh shows the whole old frame and the second the
  whole new one, as long as setup + send fit in ~33 ms. Frame rate is locked
  at 29.5 fps. The status log reports late frames, max send and max `prep`.
- Eyelids: rows are split into black / clear / lid-edge spans so only pixels
  near a lid edge run lid maths (this is what keeps blinks inside the budget).
  Lid edges use the same neon line as the outline; outline and lid lines are
  always at full brightness. Fully open lids are skipped entirely.
- Panel expects big-endian RGB565 and even x/y ranges.

### Audio (`audio_analysis.c`)
- 16 kHz, 512-sample frames (32 ms), own radix-2 FFT (pure C, host testable).
- **Automatic mic gain** (ES7210, 0–36 dB in 3 dB steps): drop fast near
  clipping, raise every 0.5 s while quiet. All analysis runs on levels with the
  gain removed; frames buffered across a gain change are skipped.
- **Sleep** needs a genuinely quiet room: the 1.5 s average level below
  `QUIET_DB` (-86.5 dB, set from on-board measurements: silent room -89 dB,
  faint background music -74 to -84 dB) for 20 s with no steady tempo. Any
  sound 3 dB above that, a steady beat, or a bump wakes it.
- **Beats**: the small MEMS mic barely hears the kick drum's bass at normal
  volume (a real recording gave 0 beats with the old bass-only detector). Onsets
  are measured across five bands (40 Hz–8 kHz, weighted toward the highs), each
  relative to its own recent level; threshold mean + 1.0σ; beats only fire while
  the last ~4 s of onsets are periodic (rhythm gate 0.4), which keeps crowd
  noise, held chords and talking out. Beats follow strong onsets, which can
  include accented off-beats.
- **Tempo**: median gap between beats, folded into 100–200 bpm so every target
  genre reads at its natural tempo (dubstep half-time 70 reads 140).
- A true tempo tracker (autocorrelation + comb phase) was tried and abandoned:
  busy 16th-note hi-hats make every grid multiple look periodic.

### Motion (`motion_analysis.c`)
- Gravity by low-pass (seeded from the first plausible reading). The pupil is a
  mass on a spring pushed against linear acceleration and tilt, with a bias to
  look down toward the crowd (at rest the accelerometer reads +1 g *up*).
- **Twist** (rotation about gravity) is separate from tilt. When motion is
  mostly twist (dominance 0.45–0.60 share, recognised within ~50 ms), the
  left/right response reverses and the twist steers the pupil straight into the
  turn, bypassing the spring so it keeps up with fast twisting. "Sideways"
  follows the true horizon (the board is rolled). Twisting accelerates the
  offset eye: the centripetal pull (-ω²·x along the screen's horizontal, with
  x = +0.12 m starboard, -0.12 m port) is removed from the accelerometer; the
  tangential push goes through the screen and is ignored. The recording showed
  twist acceleration through the screen (R² 0.92), which is how the forward
  offset was confirmed (an earlier version wrongly assumed an outward offset).
  Tuned from a recording of real twisting; gyro range is ±2048°/s (fast
  twists reached 550°/s).
- **Dance**: autocorrelation of vertical/horizontal acceleration (period
  0.3–1.2 s), with a bonus when it matches the music's tempo.
- **Activity**: rms rotation (150→300°/s) or acceleration (0.15→0.30 g) over
  ~1 s. From the recording: slow twisting 70–120°/s, fast 270–450°/s.

### Eye behaviour (`eye.c`)
- States: awake → drowsy → asleep → waking (wide-eyed, then a blink).
- **Hype** (energetic mode): driven by dancing *or* vigorous motion (activity),
  held 2 s after motion stops, then fades. In hype: rings flow outward one per
  beat, colours cycle faster, more wobble and glow, bigger beat thumps, spirals
  spin twice as fast. (This replaced an earlier "happy squint".)
- Colour drifts over time and is pulled toward the music's bass/treble balance.
- **Preset swaps** hide behind a deliberate blink (close 0.14 s, hold 0.08 s
  while the preset changes, open 0.2 s), also from asleep; not while waking.

### Presets (`presets.c`)
18 presets, cycled every `PRESET_CYCLE_S` (10 s), alternating ring and spiral
styles: Neon (the original), Hypno, Inferno, Vortex Sunset, Abyss, Hypno Ice,
Synthwave, Vortex Ultraviolet, Toxic, Hypno Acid, Aurora, Vortex, Frost,
Hypno Ember, Prism, Vortex Jungle, Hypno Candy, Vortex Magma.
- Presets change only how the eye is drawn (palette, rings, bands, fill, flow,
  pupil size, ripples, spiral arms/twist/spin); behaviour is shared.
- Spiral families (`HYPNO_FAMILY` 3 arms, `VORTEX_FAMILY` 5 arms) share
  everything but the palette, and share one spiral map (~0.74 MB PSRAM each,
  built at boot, so the eye starts ~4.2 s after power-on).
- Spiral spin is locked to the tempo: a third of a turn per beat (one Hypno
  arm per beat), calm spin 0.3 turns/s with no tempo, doubled in hype, eased
  over 1 s; each spiral preset starts at its own speed when selected.

### Controls
BOOT button cycles brightness 30/60/100% (saved to NVS; default 100%).

## Debugging with real data

The most effective tuning came from recording real data on the board and
replaying it through the pure-C code on the Mac:
- Temporary firmware builds (reverted afterwards) dumped 15 s of raw mic audio,
  or 25 s of IMU data on a BOOT press, as base64 over serial.
- `NIBBLES_PCM=<file>[:gain_db] test/host/build/test` runs mono int16 16 kHz
  audio through the analysis (beats, tempo, on-tempo %).
- `NIBBLES_IMU=<file> test/host/build/test` replays float32 acc(g)[3] +
  gyro(dps)[3] at 200 Hz through the motion analysis.
Recordings are kept out of the repo.

## Known limits and open items

- Beat detection may pulse on accented off-beats in busy tracks, and a held
  chord can give the odd stray pulse (~6/min); stricter gating cost real kicks.
- The spiral spin matches the tempo but is not phase-locked to the beats.
- The rhythm gate needs ~4 s of sound before beats start.
- `QUIET_DB` was set in the user's room; a different space may need tuning.
- The IMU's horizontal (x) sign for linear sway was never checked separately
  on the mount; twist direction was confirmed by feel on the starboard eye.
- The two eyes run independently: presets cycle on each board's own clock and
  blinks are random, so they are not synchronised.

## History (this session)

1. Identified the board, pins and PSRAM mode; built the firmware from scratch.
2. Renderer: 10.7 → 31 fps (precomputed distance maps), then TE-synced to stop tearing.
3. IMU axis mapping fixed from bench tests; look-down bias sign fixed.
4. Hype mode replaced the happy squint; full brightness by default.
5. Automatic mic gain; sleep reworked twice, ending with an absolute quiet threshold from measurements.
6. Lid edges drawn with the outline's line; lid rendering made cheap enough for the frame budget.
7. Beat detection rebuilt from a real recording (multi-band + rhythm gate); tempo ranges widened for the target genres.
8. Twist handling (into-the-turn reversal, 120 mm swing removal, horizon alignment) tuned from an IMU recording.
9. Preset system: 18 presets, blink transitions, tempo-locked spirals, motion-driven hype, faster per-frame setup.
