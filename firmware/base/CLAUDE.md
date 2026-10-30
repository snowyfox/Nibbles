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

## Version 2 (2026-10-28): power board, keypads, Live / Standby

The base now has three jobs on top of the radio work below, each in its own
file. Until the hardware exists, all of it can be driven from USB serial.

- **Power board** (`main/power.c`, `power_math.c`): its own I2C bus on the
  3.49's header (SDA 1, SCL 2, INT 3; dock detect on 5; 100 kHz, shared with
  the keypads). Finds and re-finds each chip every 2 s:
  - **BQ25798** (0x6B): at start-up turns the watchdog off (it would reset
    ICHG and EN_CHG), D+/D- detection off, writes 16.8 V / 2.0 A
    (`CHG_VREG_MV`, `CHG_ICHG_MA`), optionally lifts the input limit above
    the ILIM_HIZ pin's 2 A (`CHG_IINDPM_MA`, 0 = leave it), turns the ADC on,
    and **reads VREG and ICHG back; if they don't match it switches charging
    off** (EN_CHG = 0) and the screen says "Charger NOT set up". Reads the
    status, faults and ADC (VBUS, IBUS, VBAT, IBAT, VSYS, TS, die) every
    200 ms. TS is turned into the cells' temperature for the board's
    5.23k / 30.1k divider and 103AT NTC.
  - **INA226** (0x40, checked by its maker and die IDs): 2 mOhm shunt, 1 mA
    per bit, 16 averages: the lights' voltage, current and power.
  - **TCA9534** (0x20): P0 LIGHT_OFF, P1 PACK_OFF (outputs, written low = on
    before they're made outputs), P3 EFUSE_PG. PACK_OFF is only ever set
    while docked.
  - The pack's charge (`pack_percent`: a 21700 resting-voltage curve,
    corrected for sag at 100 mOhm) and the runtime at the present load.
  - Broadcasts `NL_MSG_BATTERY` every 200 ms (the INA226's voltage and
    current) for WLED's battery-aware current limit. **`sim on` makes up
    readings for the screen; those are never broadcast.**
- **Keypads** (`main/keypad.c`; boards in `hardware/keypad/`): TCA9534A
  expanders at 0x38-0x3F, read every 10 ms, debounced over two reads,
  re-found every 2 s; boards that stop answering release their keys. Which
  switch is which key, and the boards' order on the LED chain, is
  `KEYPAD_BOARDS` in `config.h`. A board can also be a PCF8574(T) module
  (`KEYPAD_PCF8574`: no registers, read raw; no LEDs) for breadboard
  prototypes; one at 0x27 is listed. The SK6812 LEDs run from GPIO 43 (RMT,
  `espressif/led_strip`), scaled by `KEYPAD_LED_LEVEL` (48/255) to keep the
  cable's 5 V current low. GPIO 43 is UART0 TX, so the ROM's boot banner
  can flicker them at power-up.
- **Live / Standby** (`main/controls.c`, no ESP-IDF calls, host-tested in
  `test/host`): like a video switcher. Live is what the shark shows (from
  telemetry); Standby is staged.
  - Targets ALL / BODY (WLED) / EYES / FINS: tap to select, hold for just
    the keys pressed meanwhile. **FINS** works like the others with fin
    presets (WLED presets 81-96, the fins' own segment; see
    `wled/README.md`, "Fin presets"): numbers bump them, LATCH keeps them.
    **ALL** is the whole shark: body and eyes on that preset and the fins
    following the body (fin preset 0; while a number is held, a fins bump
    to preset 80 shows that). Fins get their own bump slot.
  - **Everything is a bump until it's latched** (the user's design,
    2026-10-28): holding a number key shows that preset on the target
    (page 2: 9-16) and it goes back on release. Tap **LATCH** while holding
    it to keep it: commands go out, then the bumps let go, and the old Live
    drops into Standby.
  - **TAKE** held shows Standby (bumps) and goes back on release; tap LATCH
    while holding it to keep the swap (Standby then holds the old Live, so
    TAKE + LATCH again goes back).
  - **PAGE** cycles what the numbers are (2026-10-29): presets 1-8,
    presets 9-16, **palettes** and **layers** (the page name is in the target
    box; the PAGE key's LED is white / blue / magenta / teal).
    - **Palettes:** the 8 WLED palettes in `CTL_DEFAULT_PALETTES` (Party,
      Lava, Ocean, Rainbow, Icefire, Magenta, Orange & Teal, Atlantica) go on
      the target's own layer. The effect keeps running.
    - **Layers:** the layer looks (WLED presets 101-108,
      `NL_LAYER_PRESET_BASE` + n) go on the target's overlay.
    - **Which layers the target means:** BODY means the body's layer and
      overlay (WLED layer slots 0 and 2). FINS means the fins' (slots 1 and 3,
      which show only while the fins run a fin preset). ALL means the body's,
      plus the fins' while they run their own preset. EYES does nothing on
      these pages.
    - **Keys work as for presets:** hold to bump (`NL_BUMP_PALETTE`,
      `NL_BUMP_LAYER`; WLED puts the slot back exactly on release), tap
      LATCH while holding to keep (`NL_OP_SLOT_PALETTE`, `NL_OP_SLOT_LOOK`).
      LATCH on the layer that's already live switches the overlay off.
    - **What's live:** WLED reports its slots twice a second
      (`NL_MSG_WLED_LAYERS`), and the live palette or layer's key is red.
      The toggle is decided against the layers from before the number went
      down, because WLED reports a layer bump as live.
    - **Standby** (2026-10-30): LATCH held first, then a number, cues that
      palette or layer look in Standby (`ctl_t.standby_layers`) for the
      target, unseen; a layer look Standby already has is cued off. A new
      preset cued in Standby clears its side's cues (it brings its own
      palette and layers). TAKE previews and takes them with Standby's
      presets: a preset alone is still a preset bump; with a cue it's one
      `NL_BUMP_SCENE` bump per side (body or fins: preset if new, palette,
      layer look), and LATCH sends the preset, then the palette, then the
      look. Standby then holds Live as it was (preset, palette, layer).
      Latching a palette or layer look live also drops the old one into
      Standby, as presets do. LATCH + TAKE clears the cues. The keys show
      the Standby palette or look green, both amber.
  - LATCH held first, then a number: cue that preset in Standby without
    showing it. LATCH + TAKE: Standby = Live. PAGE + number: brightness in
    eighths. FLASH and BLACKOUT: bumps while held. FIN SENSOR: fins follow
    on / off, live.
  - Hold UNLOCK 1 s to lock or unlock (`KEYPAD_AUTOLOCK_MS` locks after idle).
  - Key LEDs: the target's Live preset red, Standby green, both amber; the
    selected target in its colour; TAKE blinks green while Standby differs; LATCH blinks while there's a
    held number or TAKE to keep.
  - Bumps now have a slot per receiver (eyes, WLED; each filters on its
    target bit), so the eyes and WLED can bump different presets at once.
    The pads, the bump button and serial bumps use the same slots.
- **Main screen** (page 0, portrait): a power status bar at the top, like a
  phone's (2026-10-28), with two iOS-style batteries (thin translucent
  outline, round cap; the fill white, iOS green with a bolt while charging,
  iOS red at 20 % or less):
  - left, the **LEMO input**, told apart by its voltage (`input_kind`,
    host-tested): up to 17.5 V an external 4S battery (at most 16.8 V),
    shown as a battery with its charge from the same 4S curve
    (`pack_percent` on VBUS and IBUS); above it a USB-C PD charger through a
    20 V trigger board, shown as the USB symbol and its voltage; nothing
    connected, a dim empty outline and "--";
  - right, the **internal pack**: its charge and battery.
  Under them the **flow line** (2 px): the LEMO's current, green for the
  charger's share (to the pack), blue for the light bus's (the lights and
  the base); all grey and pulsing when the pack is **starved** (plugged in,
  not full, charging under 100 mA, or the LEMO so far below the pack that
  the pack feeds the lights through its ideal diode's body diode); dim grey
  with nothing plugged in. Then row 2: left, the LEMO's `##.#V #.#A`, or
  with nothing plugged in the time left on the pack as hhHmmM (00H00M ..
  99H59M); right, the lights' `#.#A ##W`. Row 3: `##°C / ##°C` (cells /
  charger die) left, the pack's `##.##V` right. Temperatures are grey,
  orange near their limit and bold red past it (`cell_temp_level`,
  `die_temp_level`, host-tested; cells orange from 45 °C and below 5 °C, red
  from 55 °C and at 0 °C or below; die orange from 85 °C, red from 105 °C;
  bold is the label drawn twice, 1 px apart). Neither the LEMO's nor the
  pack's whole current is measured (the light bus bypasses the charger):
  `power_flow` (power_math.c, host-tested) works them out from IBUS, IBAT
  and the INA226, plus `BASE_LOAD_MW` for the base. The card is just those
  three rows (62 px). A problem (power board not found, charger not set
  up, lights switched off, eFuse tripped, a charger fault, cells too hot or
  cold to charge) replaces row 3 with its message in red and gives the card
  a red border; made-up readings (`sim on`) give it a thin magenta border.
  With nothing plugged in, the LEMO battery is a dim outline with no
  number. Every number in the bar uses `font_tab12`: Montserrat 12
  with fixed-width digits and a digit-wide figure space for padding (no
  kerning), so values don't shift as they change. `sim on` alternates a minute on an
  external battery (15.6 V; starved for half a minute) or a USB-C supply
  (20.1 V) and a minute on the pack, and sweeps both temperatures through
  their levels. Then LIVE (red; a fifth row, LOOK: the body's palette, by
  name if it's one of the palette page's, and "+ Ln" for a layer over it),
  STANDBY (green; rows that differ bright; tap it = TAKE), and the target
  box (tap = next target), page name, LATCH, lock, links, keypads found, the
  event line. STANDBY has a LOOK row too (as TAKE would leave it: "--"
  where a new preset brings its own palette) and the "hold TAKE" line
  under it. Positions: card 4, LIVE 72 (157 tall), STANDBY 235 (157 tall),
  target 398, lock 454, links 476, keys 494, event 516. Page 1 is the
  preset grid, page 2 the old cards and bump pads.
- Serial for all of it: `key NAME [down|up]` (a tap without down/up; `keys`
  lists the names: 1-8, all, body, eyes, fins, page, latch, blackout,
  finsensor, take, flash, unlock, latch), `sim on|off`, `lights on|off`,
  `power` (logs the power board), `page 0|1|2`, `leds on|off` (prints
  `LEDS rrggbb,...` in key order whenever the key LEDs change; the simulator
  uses it to drive this base from the computer's keyboard).
- Host tests: `make -C firmware/base/test/host`.

Earlier features, still there (`main/main.c`):
- Scans for the WLED usermod's anchor heartbeat like the eyes
  (`BASE_IS_ANCHOR 0`). If none is heard for 12 s (`BASE_ANCHOR_FALLBACK_MS`)
  it anchors channel 6 itself, so the eyes still have a channel. It then spends
  300 ms on another channel every 3 s, in turn, looking for WLED, and hands
  over as soon as it hears it. Commands and bumps end a peek at once and put
  the next one off by 3 s, so held bumps never miss keepalives.
  `BASE_IS_ANCHOR 1` makes it a fixed anchor.
- **Status screen** (`main/display.c`, LVGL 9.2 + `esp_lcd_axs15231b` from the
  component manager): portrait 172×640 by default (`DISPLAY_ROTATION` 0 or
  180; cards stacked, big FLASH/BLACKOUT pads along the bottom) or landscape
  640×172 (90 or 270; cards side by side). Preset names use a 20 px font and
  wrap (portrait) or end in "..." (landscape), never scroll. Three cards (EYES: preset, state,
  link, both fps, bpm, hype bar; WLED: on/preset, brightness, effect, palette,
  fps, LEDs, brightness bar; RADIO: channel, anchoring/locked/scanning,
  counters) and an event line (last command result, held bump, which also
  outlines the screen). Stale cards grey out after 2 s. `DISPLAY_ROTATION`
  picks the layout and which way up. Panel notes: frames go out whole in 64-row QSPI chunks
  (the AXS15231B takes whole frames in order); LVGL renders full-frame in
  the chosen orientation and `flush_cb` copies (rotation 0) or rotates into a separate buffer and byte-swaps that
  (never the LVGL buffer, which LVGL keeps between frames); set the rotation
  before `lv_display_set_buffers` (the stride comes from the width); LVGL's
  printf has no `%f`.
- **Touch** (AXS15231B touch at 0x3B on I2C SDA 17 / SCL 18, read with the
  vendor's 11-byte command; native coordinates, LVGL rotates them): tap the
  EYES or WLED card for that side's next preset; long-press the EYES card to
  hold the current preset or let them change by themselves again (the card
  shows `auto` or `held`); drag the slider at the
  bottom of either card to set its brightness (sent on release); hold the FLASH or BLACKOUT
  pad to bump (targets `BTN_BUMP_TARGET`; BLACKOUT wins over FLASH). Taps go
  through a queue to an actions task, since commands block for their ack; the
  buttons task runs one bump state machine for the key and both pads.
- **Preset page** (swipe left; swipe right to go back; an LVGL tileview):
  square buttons 1-10, 2 columns by 5 rows in portrait (5 by 2 in landscape).
  Tap N sets preset N on WLED (preset id N) and the eyes (index N-1). Hold N
  bumps preset N+10 on both while held (`DISPLAY_PRESET_HOLD_OFFSET`). The
  eyes have 18 presets, so holds of 9 and 10 only change WLED. At the top:
  an AUTO switch (eyes change presets by themselves; turns off when a preset
  is picked) and a Preset / Beats / Peaks selector (eye sound reactivity:
  each preset's choice, or all on beats / on every peak). Both follow the
  eyes' telemetry flags, but not for 1.5 s after being touched. The sliders
  don't start a page swipe (they don't chain scrolling to the tileview).
- `shot` over USB serial (or `python tools/base_shot.py PORT out.png`) prints the next frame as base64 RGB565 (`SHOT 640 172
  rgb565le` … `SHOT END`) to check the layout from a computer. `shot rle`
  sends it run-length encoded (`rle565le`: runs of count - 1, then the
  pixel, little-endian; a few percent of the size), as the simulator uses.
  Both print from their own task, so the console keeps reading keys.
- Logs the eyes' and WLED's telemetry every 2 s.
- When WLED shows a stream from a main board (`NL_MSG_WLED_STREAM`), the WLED
  card shows "Streamed", the source address, and the stalls per minute and
  longest gap; the log line adds slow gaps and stalls since boot.
- BOOT = next eye preset. Second key (GPIO 16) = a **flash bump** for the
  eyes and WLED (`BTN_BUMP_TARGET`) while held (START, HOLD every 100 ms, STOP; WLED releases by itself 300 ms after
  the last message).
- USB serial commands, for testing: `next`, `prev`, `set N` (eyes);
  `wled next`, `wled set N`; `wled slot look S N` (WLED layer slot S takes
  layer preset N, 0 switches it off), `wled slot pal S N`; `bri N`, `wled bri N` (0..255, or `+N`/`-N` to
  step; both set WLED's brightness, which the eyes follow, as does the eyes
  card's slider); `auto on|off`; `preset N` (as a grid tap); `page 0|1`; `react preset|beats|peaks`; `fins on|off|toggle` (WLED's fin
  LEDs follow the fins, or stay at the modelled angle); `shot`; `[eyes|wled] bump flash MS`,
  `[eyes|wled] bump black MS`, `[eyes|wled] bump preset N MS` (no prefix:
  flash and blackout go to both, preset bumps to WLED, as the preset numbers
  differ). Commands are unicast with an ack and up to 3 tries of
  60 ms.

Partition table: ESP-IDF's "single app, large" (1.5 MB app); the app outgrew
the default 1 MB one when the preset page was added.

Build: `idf.py -C firmware/base build`, flash to the base board's port
(check its serial number, 28:84:85:91:27:08, first).
