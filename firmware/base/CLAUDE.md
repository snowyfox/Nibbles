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

## Two boards: the 3.49, or a 1.91 with two 1.64 monitors (2026-11-14)

The 3.49 is too big for the base box (the user, 2026-11-14), so the base is
moving to a **Waveshare ESP32-S3-AMOLED-1.91** (MAC 3C:0F:02:D9:7A:08, the
old preview screen; a second one, due 2026-11-15, is to be the spare) plus two
1.64 monitors for Live and Standby (`firmware/monitor`). The 3.49 build
stays as the fallback until the new set has run a rehearsal.

- **Which board** is a menuconfig choice (`main/Kconfig.projbuild`):
  `CONFIG_BASE_BOARD_LCD_349` (default, `build/`) or
  `CONFIG_BASE_BOARD_AMOLED_191` (`build-191/`):
  `idf.py -B build-191 -D SDKCONFIG=build-191/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.board_191" build`
  (then `-p PORT flash` with the same `-B` and `-D SDKCONFIG`).
- **1.91 pins** (`config.h`): power-board I2C SDA 2 / SCL 3 (header H1 6, 7;
  GPIO 1 has the battery ADC's 100k divider), INT 11, DOCK_UART 12 (unused),
  dock detect 13, key LEDs 14 (no boot banner on it), TF card 1-bit SD CLK 9 / CMD 42 / D0 8 (the
  board's "V2" wiring in Waveshare's SD example; the older wiring shared the
  screen's clock, GPIO 47). Screen: CS 6, SCLK 47, D0 18, D1 7, D2 48, D3 5,
  RST 17. 33-37 reach the header but are the octal PSRAM's.
- **Power board cable** (its J10, JST-GH 8, as for the 3.49; 1.91 headers per
  Waveshare's schematic, H1 = the row with GP43 at pin 1, H2 = the row with
  VBUS at pin 1; go by the silkscreen labels, not by counting):

  | J10 | Signal | Colour | 1.91 | Note |
  |---|---|---|---|---|
  | 1 | +5V_349 | red | H2-1 VBUS | the USB-C 5 V, as on the 3.49; the power board feeds it through D5 |
  | 2 | GND | black | H1-3 GND (or any GND) | |
  | 3 | +3V3 | orange | H2-5 3V3 | the 1.91's 3.3 V out, for the bus pull-ups. **Not H2-4 (3V3_EN)**: low there switches the board off |
  | 4 | SDA | yellow | H1-6 GP2 | |
  | 5 | SCL | grey | H1-7 GP3 | |
  | 6 | INT | white | H1-9 GP11 | |
  | 7 | DOCK_UART | green | H1-10 GP12 | unused by the firmware (`PIN_DOCK_UART`) |
  | 8 | DOCK_DET | purple | H1-11 GP13 | |

  The keypad LEDs' data (not on J10) goes to H1-12 GP14. The colours are the
  ones every Nibbles GH cable uses (`hardware/keypad/README.md`, "Wire colours").
- **No clock chip** on the 1.91 (Waveshare's "onboard RTC" is the ESP32's
  own): `rtc.c` skips the PCF85063 (`BASE_HAS_RTC_CHIP 0`); the time comes
  from `time` or a WLED's `NL_MSG_TIME`, as with the 3.49 after a power-off.
- **DETAIL key** (`KEY_DETAIL`, 2026-11-14; on the third key board since 2026-11-16): the
  screens are read from half a metre or more, in the rain, with no touch, so
  each normally shows a **simple page** of big text, and DETAIL flips all three
  (this one and both monitors, `NL_SHOW_DETAIL`) to the small-text page: held,
  while held; a tap (under `CTL_DETAIL_TAP_MS`, 400 ms) keeps it there until the
  next tap. It works with the keys locked. Its LED is white while the details
  show. On the 3.49 it shows page 1 (Live and Standby). Serial: `key detail`.
- **Simple page** (2026-11-14, `build_simple` / `simple_update`; 48 px font in
  `sdkconfig.board_191`): nothing under 20 px. **Since 2026-11-21 the battery is
  at the bottom** and the event line at the top (the top centimetre is hard to
  see through the box's cutout), then what's missing, the target box, a problem.
  The bottom 252 px: PACK % and bar, **EXT** % and bar (the battery on the LEMO,
  `input_pct`; "USB-C" for a supply, "--" with nothing), then TIME LEFT (or
  CHARGING / PLUGGED IN) and LIGHTS side by side in 28 px. Pack % (48 px, coloured) with a
  bolt (charging) or the USB plug (on a supply) on the right; a charge bar; TIME
  LEFT on the pack, or CHARGING and its current, or PLUGGED IN with Full / Weak /
  Idle (40 px); LIGHTS in watts or POLE OFF; the worst power problem in a red box
  ("No power board" without one); the keypad's target in a 48 px box with LOCKED,
  LATCH or what the numbers are; "No WLED, eyes, keys" for what's missing; the last
  event for 8 s (`SIMPLE_EVENT_MS`).
- **Detail page** (`main/display_191.c`, same `display.h` API): the first
  **portrait** layout (240 x 536, the panel's own orientation; the user mounts it that way;
  `DISPLAY_191_UPSIDE_DOWN` turns it round), power at the top: pack % (40 px), voltage and charger state; the LEMO input;
  lights A and W; time left; cell and charger temperatures, or the worst
  problem in red with a red border; the pack's charge over the last hour.
  Under it: the keypad's target box with page name and LATCH / LOCKED, Live and
  Standby by name, links/channel/keypads, the log line and the event line. No
  touch; the sharks, fins, eyes, palette viewer and survey page are left out
  (their functions are empty). `shot` works (240 x 536).
- **Clock from a phone over Bluetooth** (`main/ble_time.c`, 2026-11-14; only
  builds with NimBLE, the 1.91's `sdkconfig.board_191`): with no time at boot
  the base advertises as "Nibbles Base" (`BLE_TIME_NAME`) for up to 10 min
  (`BLE_TIME_WINDOW_MS`). A phone that connects is paired (Just Works, bonds
  in NVS) and its Current Time Service (0x1805: Current Time 0x2A2B, Local
  Time Information 0x2A0F; iOS provides it itself) is read and turned into UTC;
  then the link drops and NimBLE is stopped and deinitialised, so ESP-NOW has
  the radio again. It also stops when the time comes another way. Serial:
  `ble on` (forget the time, open the window again), `ble` (state). Event line:
  "Bluetooth: connect to ...", "phone connected", "Clock set from the phone".
  The GATT client needs `CONFIG_BT_NIMBLE_ROLE_CENTRAL` in ESP-IDF's Kconfig;
  NimBLE's buffers are in PSRAM and the screen's draw bands are 20 rows, or
  the logger and keypad tasks can't start (internal RAM: 49 KB free after).
  **It also looks like a media remote** (HID over GATT, consumer control,
  one 16-bit report it never sends, plus battery and device information
  services; advertised with the HID service and appearance 0x0180): iOS lists
  only some kinds of accessory in Settings > Bluetooth and reconnects only
  those by itself. Soliciting Apple's notification service (ANCS), tried
  first, did not list it; LightBlue worked but needed a tap at every power-up.
  **Tested 2026-11-14 with the user's iPhone:** pair once from Settings >
  Bluetooth ("Nibbles Base" under Other Devices); after that, with the phone
  nearby (it was **locked** the whole time), a restart is set with no touch: the phone reconnected 1.3 s
  after advertising began, the clock was set 3.6 s after power-up, Bluetooth
  off at 3.7 s. The phone then shows it "not connected", and it can't be
  reconnected by hand until the board advertises again (next boot, or
  `ble on`): that's by design. The on-screen keyboard: no problem seen, but
  none was open during the test. iOS hides it for a connected keyboard-type
  HID device; this one is a consumer control (media keys), and it's connected
  for about a second, so at worst a keyboard open at that moment could blink.
- **Live / Standby to the monitors**: the main loop broadcasts `NL_MSG_SHOW`
  (`nl_show_t`, one packet per side) every 500 ms and on a change
  (`show_broadcast` in `main.c`), on both boards. The monitors' mode, by side
  (Live, Standby) or by brain (Right, Left), goes with it (`NL_SHOW_BY_BRAIN`):
  the **MONITORS** key (`KEY_MONITORS`, tap to toggle; LED grey / blue; on the
  third key board since 2026-11-16) or `monitors [sides|brains|toggle]`; kept in NVS
  (`base`/`mon_brain`). See `firmware/monitor/CLAUDE.md`. **Since 2026-11-23 the
  base always starts by brain** (the user: each monitor fixed to one brain, the top
  one the Left Brain); the saved setting is ignored, `monitors` still changes it
  until a restart.
- Tested 2026-11-14 on the bench with nothing attached: boots, screen drawn
  (screenshots), the monitors hear it. TF card: with a card in, it mounts and
  logs (L00014, 2026-11-14; the 0x107 timeout before was an empty slot). Power board and keypads not
  connected to the 1.91 yet.

## Version 2 (2026-10-28): power board, keypads, Live / Standby

The base now has three jobs on top of the radio work below, each in its own
file. All of it can also be driven from USB serial.

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
  - **On a USB-C supply, charging gives way to the lights** (2026-11-01,
    `charger_input_limit_ma`, not yet run on a real charger): the dongle's trigger
    board gives 5 A at 20 V, so IINDPM (REG06, re-written after a plug-in) = 5 A less
    400 mA margin less the lights' peak over 3 s, between 100 mA and the ILIM_HIZ pin's
    2 A. The pin stays the upper limit. Lights alone over 5 A can't be helped from
    here: see `docs/power.md`. In the log as `chg_limit_ma`.
  - **External battery first** (2026-11-23, after the festival: the pack ran the
    lights while a VB99 only fed the charger's 2 A): with a battery on the LEMO at
    least about as high as the pack, the base switches the pack's ideal diode off
    (`PACK_OFF`, `pack_should_be_off`: off at VBUS >= VBAT - 0.2 V, on again below
    VBAT - 0.4 V, before the pack FET's body diode would carry the lights), so the
    lights run from the LEMO and the pack only charges. The body diode still catches
    the bus if the cable is pulled. And the charger's input limit is the register's
    alone (EN_EXTILIM off, so the ILIM_HIZ divider's 2 A no longer caps it), up to
    3.3 A: on a battery 10 A (a D-Tap) less 0.5 A less the lights' 3 s peak while
    the pack is off; on a USB-C supply as before (5 A less margin less the lights).
    Host-tested; **not yet tried on the power board** (needs the external battery):
    check the lights run from the LEMO (pack current ~0 or charging), pulling the
    cable doesn't blink them, the pack FET stays cool with the LEMO below the pack,
    and the D-Tap current stays under 10 A. `power` logs `lights on, pack OFF`.
  - The pack's charge (`pack_percent`: a 21700 resting-voltage curve,
    corrected for sag at 100 mOhm) and the runtime at the present load.
  - Broadcasts `NL_MSG_BATTERY` every 200 ms (the INA226's voltage and
    current) for WLED's battery-aware current limit.
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
- **Key assignment and the mirror panel** (2026-11-16): three 8-key chain
  boards, 0x38 / 0x39 / 0x3A, no key_upper. The assignment in `KEYPAD_BOARDS`
  is a placeholder (third board: TAKE, FLASH, UNLOCK, PALETTE, LAYER,
  MONITORS, DETAIL, one free) and the user expects to change all of it later:
  that table is the only place it lives. The **mirror panel** (an Adafruit
  NeoPixel FeatherWing, 4 x 8 WS2812B, at the end of the LED chain on the last
  board's J2) repeats every key's colour so the keys, facing away from the user,
  can be read from behind: `KEYPAD_PANEL_*` in `config.h`, pixels after the
  keys on the same strip, or before them with `KEYPAD_PANEL_FIRST 1` (the
  panel's own 74AHCT1G125 then buffers the data for the hub-version key boards). Each pixel row is a key board in chain order and each
  pixel along it a switch (`keypad_panel.h`, host-tested), so it follows the
  boards, not the key names; flips for how it's mounted, its own level
  (`KEYPAD_PANEL_LEVEL`, 16). A board the base doesn't find leaves its row
  dark (2026-11-19), so a gap means a cable is out.
  **On hardware 2026-11-19** (the 1.91 base box, one key board 0x38 plus the
  wing, wing first in the chain: `KEYPAD_PANEL_FIRST 1`): `FLIP_ROWS 0`,
  `FLIP_KEYS 1` match how they're mounted (the user confirmed). 0x38 sits on
  the right of the keypad and is the wing's right-hand column, so 0x39 and 0x3A
  must sit in the middle and on the left; the wing's leftmost column stays dark
  with three boards. The user may want the layout changed later (e.g. skip a
  column, or per-board positions) once the hardware is done.
  **2026-11-21:** the boards are mounted with switch 1 at the bottom, so
  each board's list in `KEYPAD_BOARDS` now runs bottom to top (the columns
  read top-down as designed: 1-8, ALL...BLACKOUT, TAKE...DETAIL). UNLOCK and
  FIN SENSOR were taken off the keys (their switches are free), so the
  keypad can't be locked from the keys; the mirror panel follows the switches
  and needed no change.
- **One-handed layout** (2026-11-21, the user's design after the first festival
  night: the old layout's modes, held numbers and LATCH were too much one-handed).
  `KEYPAD_BOARDS` from the top: left column **L1-L8 = the Left Brain's numbers**,
  right column **R1-R8 = the Right Brain's**, middle **SET/BUMP, SWAP, FLASH,
  BLACKOUT, PAGE, DETAIL, TARGET, BANK** (the user's order, 2026-11-21; FLASH
  lit white, BLACKOUT dark until pressed; the bottom four in colours of their own: PAGE teal / amber, DETAIL purple (white on), TARGET ALL yellow / BODY pink / EYES cyan, BANK red presets / magenta palettes;
  `controls.c`, "one-handed layout"; host test `test_one_handed`). MONITORS is
  off the keys (`monitors` on the console still works).
  - **PAGE** (a tap): the columns' next 8 numbers, for both columns: presets
    1-8 / 9-16 (fins 81-88 / 89-96; `ctl.num_page`, two pages while
    `CTL_PRESETS` is 16), or with palettes favourites 1-8 / 9-16.
    White on page 1, amber on page 2; the screen's note says
    "SET 1-8", "BUMP 9-16", "SET PALETTE".
  - **SET/BUMP** (orange SET, blue BUMP; starts in SET): SET keeps a number at
    once (no LATCH), BUMP shows it only while held (the old bump stack).
  - **TARGET** cycles ALL (white) > BODY (pink) > FINS (orange) > BODY+FINS
    (violet, `TGT_BODY_FINS`: the body preset with the fins following, ALL
    without the eyes) > EYES (cyan).
  - **BANK**: presets (white) or palettes (magenta): **16 favourites**, 8 per
    PAGE (`ctl.favs`, `CTL_DEFAULT_FAVS`: page 1 Party, Icefire, Magenta, Orange &
    Teal, Atlantica, Aurora, Tiamat, Toxy Reaf; page 2 Lava, Fire, Sunset, Magred,
    Red & Blue, Candy, Pink Candy, Rainbow). **`palettes`** (USB or a brain's
    console: `/keys/ask?c=palettes...`, `base_ask.py`) lists them, `palettes Party,
    Lava, ...` replaces them (WLED names, any case, or ids; up to 16; kept in NVS
    `base`/`favpal`), `palettes reset` goes back to the defaults
    (`ctl_parse_palettes`, host-tested). **Each palette key fades through its
    own palette** (2026-11-23, `palette_fade`: its 16 colours blended, once
    round every `CTL_PAL_FADE_MS`, 3 s): the column of the brain on the shark at
    full brightness, the other brain's at 12% (`CTL_PAL_OTHER_PCT`; 40% looked no
    different on the keys); the palette a brain shows now stays solid red (on the
    shark) or green, as before the fades; **tapping it again takes the palette off**, back to the
    preset's own (the preset is sent again, which also drops a layer look); the
    mirror panel follows.
  - **A brain's column** changes Live if that brain is on the shark (or there's
    one brain), else Standby: sent straight to that brain (`ACT_*` with
    `standby`), the eyes cued for the swap; never the brain on the shark. BUMP on
    the off-shark brain previews there while held and puts Standby back after.
  - **SWAP** (the old TAKE): SET, a tap swaps the brains (`take_ab`); BUMP, the
    other brain while held. `ctl.left_live` comes from `NL_MSG_AB` (main), except
    for 2 s after a SWAP.
  - Key lights: the brain's current preset (or palette) for the target red on
    the brain on the shark, green on the other; the rest dim blue (left) / warm
    (right). The simple page's target box says the target and "SET PRESET",
    "BUMP PALETTE" etc. The monitors (not updated) show BODY+FINS as ALL.
  - The old keys (NUM, LATCH, TAKE, PAGE, ...) still work if mapped.
- **Remapping without a firmware change** (2026-11-16, `keymap` on the USB
  console; `keypad_console` in `keypad.c`, text in `keymap_text.c`,
  host-tested): `KEYPAD_BOARDS` is the default and each board's map can be
  replaced, saved in NVS (namespace `keymap`, key `k` + address in hex, the key
  names comma-separated, so a firmware update that reorders `ctl_key_t` keeps
  it). `keymap` lists every board (number, address, its 8 keys, "(changed)"),
  `keymap B S NAME` sets one switch (B: 1-N or `0x38`; NAME as `keys` lists
  them, `none` clears), `keymap B a,b,c,...` a whole board, `keymap B reset`,
  `keymap reset` (everything back to `KEYPAD_BOARDS`). The keypad task takes
  the change on its next poll, letting go of a key held on a switch that
  changed; the LEDs and the mirror panel follow. A saved map that no longer
  parses (a renamed key) is ignored with a warning. Built for both boards, not
  run on hardware yet (no base on USB).
- **Remapping from a phone** (2026-11-16): the brains' `/keys` page (WLED
  usermod, `wled/usermod_nibbles/keys_page.h`) asks the base over the radio
  with `NL_MSG_CONSOLE` (`nl_console_*` in the shared protocol: one question
  packet, the answer in parts). `on_packet` queues questions from a brain
  (role WLED or WLED_STANDBY) for the main loop (`radio_console_poll`; no task of its own, the 1.91 is short of internal RAM), which runs
  `keypad_console` with its answer collected and sends it back to that brain;
  anything but `keymap ...` and `keys` gets an ERROR line. For the page:
  `keymap colors` (KEYCOLORS: each switch's LED colour now), `keymap last`
  (KEYLAST: the switch pressed last and how long ago, to find which switch is
  which), `keymap all` (all of it plus KEYFOUND). `keys` moved from `main.c`
  into `keypad_console`. The USB console and the radio share a mutex. Built
  for both boards, not run on hardware yet.
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
    (numbers 1-16 are presets 1-16) and it goes back on release. Tap **LATCH** while holding
    it to keep it: commands go out, then the bumps let go, and the old Live
    drops into Standby.
  - **Held bumps stack** (2026-11-09, the user's design): keys held A, B,
    C, D show D; letting go of D shows C; letting go of a lower one (A or B)
    changes nothing showing and just takes it out; the last one up goes back
    to how it was. Numbers, keys of their own, FLASH, BLACKOUT and TAKE all
    stack (`ctl_t.held`, `CTL_BUMP_STACK`), each on the target it was pressed
    on. The one underneath comes back as a direct replace (its bumps, then
    END only for slots it doesn't use), so Live never shows in between.
    LATCH keeps the newest held number or TAKE and empties the stack (their
    releases then change nothing).
  - **TAKE** held shows Standby (bumps) and goes back on release; tap LATCH
    while holding it to keep the swap (Standby then holds the old Live, so
    TAKE + LATCH again goes back).
  - **Numbers, modifiers and keys of their own** (2026-11-03, the user's
    design for building a keymap; `controls.h`): number keys **1-16** (9-16
    replaced the old "9-16" page). Held modifiers change what the numbers do,
    as hotkeys on a computer: **PALETTE** + a number: a palette from the
    palette viewer (below), **LAYER** + 1-16
    layer looks, **BRIGHT** (or PAGE held, as before) + 1-8 brightness in
    eighths. A modifier alone does nothing; letting go of it while a number
    is held changes nothing. Every one of those also has a **key of its
    own** that does it whatever is held or paged: PRESET1-16, PALETTE1-8
    (8 favourite palettes, `CTL_DEFAULT_PALETTES`),
    LAYER1-16, BRIGHT1-8, and POLEPOWER (= PAGE + BLACKOUT). They bump,
    latch and cue (LATCH held first) exactly like the numbers. The new keys
    come after UNLOCK in `ctl_key_t`, so the first 19 keep their order
    (`LEDS` lines, the simulator). The screen's page name shows the
    modifier held (`ctl_mode`). Brightness keys light as a meter.
  - **Palette viewer** (2026-11-03): every WLED palette (all 72 of WLED
    16, `wled_palettes.c`, generated by `tools/wled_palettes.py` from the
    WLED-16 checkout: names and each palette's 16 colours made as WLED makes
    them) can be picked. While the numbers are palettes (PALETTE held, or the
    palette page) the screen shows `CTL_PALETTE_ROWS` rows of the list
    (**8**, for one hand; that one value in `controls.h` changes it, up to
    16, and the rows resize), numbered as the keys pick them, each with its
    name and colours; the target's Live one is marked red, Standby's green,
    a held number white. The list goes round (`ctl.pal_first`, the top row).
    **UP/DOWN** scroll it by one; held, after `CTL_SCROLL_DELAY_MS` it keeps
    scrolling every `CTL_SCROLL_REPEAT_MS`, faster after
    `CTL_SCROLL_FAST_MS` (`ctl_tick`). **PAGEUP/PAGEDOWN** (and LEFT/RIGHT)
    move a screenful. They do nothing, dark, with no palettes showing.
    Palette 0 (Default) shows Party and says "effect's own"; 1-5 are made
    from the effect's colours, so they show grey stripes. `display_palettes()`
    from `on_key`, so it appears as the key goes down.
    - **The scroll is animated**: a one-row move slides (150 ms, ease-out,
      shorter when steps come faster). The list (names and colours) is drawn
      once per move into a picture one row taller each end, and `flush_cb`
      copies it into every frame at the slide's position, so LVGL draws only
      the header and the numbers: ~28 frames a second while scrolling. While
      it shows, the pages under it are hidden, the sharks aren't drawn, and
      LVGL refreshes every 15 ms.
  - **Screen speed** (2026-11-03, measured on the log's `sharks:` line, which
    now gives frames, ms to send and ms to draw+send): portrait renders in
    LVGL's **direct** mode (only what changed is redrawn; the panel still gets
    whole frames, sent after the last area), frames go straight from LVGL's
    buffer to the panel in byte-swapped chunks, two buffers alternating (12 ms
    a frame, was 32), LVGL is built `-O2` (the rest stays `-Og`) and uses the C
    library's memcpy. The main screen went from ~11 to ~20 frames a second.
  - **PAGE** cycles what the numbers are with no modifier held (2026-10-29,
    for a keypad without modifier keys): presets, **palettes** and
    **layers** (the page name is in the target box; the PAGE key's LED is
    white / magenta / teal). Numbers with nothing to do (PALETTE + 9-16)
    are dark and do nothing.
    - **Palettes:** the 8 WLED palettes in `CTL_DEFAULT_PALETTES` (Party,
      Lava, Ocean, Rainbow, Icefire, Magenta, Orange & Teal, Atlantica) go on
      the target's own layer. The effect keeps running.
    - **Layers:** the layer looks (WLED presets 101-116,
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
    eighths. PAGE + BLACKOUT: pole power off or on, only on a USB-C supply
    (`pole_off_stays`): the eFuse opens so the pack charges with the shark dark,
    the status bar shows POLE OFF, and power returns by itself when the supply
    goes. FLASH and BLACKOUT: bumps while held. FIN SENSOR: fins follow
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
  three rows (62 px). The card's background is black. With no power board it has "???" centred on the top row
  and no border. A problem (charger not set
  up, lights switched off, eFuse tripped, a charger fault, cells too hot or
  cold to charge) replaces row 3 with its message in red and gives the card
  a red border.
  With nothing plugged in, the LEMO battery is a dim outline with no
  number. Every number in the bar uses `font_tab12`: Montserrat 12
  with fixed-width digits and a digit-wide figure space for padding (no
  kerning), so values don't shift as they change. Under it, a side view of the shark from the Live brain's preview
  stream, or a lone brain's (`NL_MSG_PREVIEW`, 164 x 82 px, `shark_pos.h` from
  `tools/sim/make_preview_map.py`); all dim with no stream. Under that, the fins
  laid flat, seen from above (leading edges up, port on the left, live colours,
  `fin_pos.h`: a 4 px band like the shark's line, both strands side by side, each pixel taking its nearest LED), a padlock between them
  (grey shackle, amber body; unlocked: shackle lifted with its right end clear,
  all at 25% opacity; dim body when not known), and an invisible touch key for FIN SENSOR across the whole band; a
  card shows behind the fins only while that key is down (`display_fin_key`).
  A second shark and fins at y 326 show the Standby brain (by the sender's role, so a TAKE swaps them; no padlock or touch key). Both draw in one screen refresh: about 11 a second, 77 ms each, the same with one stream or two (the `sharks:` log line). Under the fins, the **eyes** at y 242, 80 px each, starboard on the left, port on the right: stills of
  the preset on screen (`eye_thumb.c`, the rings, spiral arms, colours and lids of
  the eyes' `render_core.c` with no music or motion; it builds in the eyes'
  `presets.c`, so new presets just appear). Redrawn only when the telemetry's preset
  or state changes, or a look arrives (`NL_MSG_EYE_LOOK`); nothing spins and colours
  don't drift. Unlit grey when the eyes aren't heard. Test: `eyepic N [STATE]`, `eyepic off`.
  At the bottom, edge to edge, the pack's charge over the last hour at 25%
  opacity: 172 one-pixel columns (about 21 s each, newest on the right), 1 px per
  percent, green, orange at 35% or less, red at 20% or less; RAM only.
  That's all of page 0 (2026-11-01; card 0, shark 70, fins 154, 84 tall, eyes 242, graph 540). Page 1, swipe left: LIVE (red; a fifth row, LOOK: the body's palette, by
  name if it's one of the palette page's, and "+ Ln" for a layer over it),
  STANDBY (green; rows that differ bright; tap it = TAKE), and the target
  box (tap = next target), page name, LATCH, lock, links, keypads found, the
  event line. STANDBY has a LOOK row too (as TAKE would leave it: "--"
  where a new preset brings its own palette) and the "hold TAKE" line
  under it. Positions: LIVE 4 (157 tall), STANDBY 167 (157 tall), target 330,
  lock 386, links 408, keys 426, event 448. Page 2 is the preset grid, page 3
  the old cards and bump pads, kept for reference and debugging for now.
- **Two brains** (2026-11-01, host-tested, not on real boards yet; `wled/README.md`,
  "A/B brains"): when the controller's `NL_MSG_AB` says both wires are up and the
  Standby brain (`NL_ROLE_WLED_STANDBY`) is heard, `ctl.ab` is on. TAKE held then
  shows the Standby brain (a bump to `NL_TARGET_AB`), LATCH keeps it (`NL_OP_AB_TAKE`,
  `TAKE_FADE_MS`), and Live and Standby just trade places. Eyes have no second
  brain, so they're bumped and commanded as ever. `ctl_standby_sync` keeps the brain
  on Standby's preset, fin preset, palette and looks once a second, and main its
  brightness; it waits 2 s after a swap. Panels read LIVE R / STANDBY L (the Right Brain, `ab_side` 0, and the Left Brain, 1). If a brain
  goes down, everything falls back to the one-brain behaviour. Serial: `ab`, `ab take
  [MS]`, `ab hold on|off`, `wled wifi on|off` (2026-11-02: `NL_OP_CTL_WIFI` by the same
  route as a take: the controller, radio off, joins `Nibbles` until it restarts).
  **A change made on the Standby brain itself** (its WLED page: preset, fin preset,
  palette or layer look) becomes Standby's cue instead of being put back
  (`ctl_standby_adopt`, 2026-11-02, while the base has no keys; the user may want it
  reverted later): a report that changed since the last one and differs from the cue is
  taken. A cue from the base still wins (the report hasn't moved). Serial `ab take` now
  goes through `ctl_ab_take`, as TAKE + LATCH: Standby keeps Live as it was (before, the
  sync pushed Standby's preset onto the brain that had just left Live).
- **Simulated power data: removed 2026-11-09** (the real power board works).
  It was `main/power_sim.c`, a menuconfig choice and `sim on|fast|real|off`;
  see git history before then. Logs from those builds mark made-up rows with
  `sim` 1 or 2, which the charge graph skips.
- **Data logger** (`main/logger.c`, `log_format.c`; 2026-11-01): one CSV row a
  second on the TF card (1-bit SD: CLK 41, CMD 39, D0 40), a new file each boot,
  `/nibbles/L00001.CSV` and up, on its own lowest-priority task. Synced every
  10 s; retries every 30 s with no card; deletes the oldest logs below 200 MB
  free. `t_s` is seconds since boot, `time` Unix time from the PCF85063 (`main/rtc.c`,
  software I2C on GPIO 47/48; 0 if it has lost its time). The `sim` column is
  always 0 now (older builds marked made-up readings there). It never formats a card that won't mount. At boot, if the
  clock is good, the last hour of the last five files refills the charge graph
  (real readings only). The clock chip's VDD is 3V3 and the board's VBAT through
  two diodes, plus 1 uF (schematic): the prototype's 18650 on J1 keeps it going
  with the power off (tested, a 30 s+ unplug). The final build has no 18650 (user,
  2026-11-01), so it loses the time within seconds of power off unless something
  else backs it up: `time` reads 0, `Log ... no clock` on the page, the graph
  starts empty; the user accepts that. A WLED that knows the time broadcasts it
  every 10 s (`NL_MSG_TIME`), and the base sets its clock from that, then refills
  the graph: open `http://<WLED>/time` on a phone once after power-up. Don't put a tiny LiPo on J1: its ETA6098
  charger is set to 1.2 A (R13 150K) and it charges whenever VBUS is up. A coin
  cell would go through a series Schottky to C15's non-ground side, not on J1.
  Pulling the card and putting it back was tested too: a new file, the gap
  counted in `dropped`, up to 10 s of unsynced rows lost, no crash.
  `tools/base_log.py` sets the clock and fetches files over USB. Page 1 shows
  its state above the event line (event now 466).
- Serial for all of it: `key NAME [down|up]` (a tap without down/up; `keys`
  lists the names: 1-8, all, body, eyes, fins, page, latch, blackout,
  finsensor, take, flash, unlock, 9-16, palette, layer, bright, preset1-16,
  palette1-8, layer1-16, bright1-8, polepower, left, right, up, down), `lights on|off`, `pole on|off`, `log`, `log on|off`, `log tail`, `log list`, `log get N`, `time [UNIX|forget]`, `eyepic N [STATE]|off`, `ab`, `ab take [MS]`, `ab hold on|off`,
  `power` (logs the power board), `page 0|1|2|3|4`, `survey` (site survey, results in the log and on page 4), `radio move N` (move the whole network to channel N), `leds on|off` (prints
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
  counters) and an event line (last command result, held bump). Stale cards grey out after 2 s. `DISPLAY_ROTATION`
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
  card's slider); `auto on|off`; `preset N` (as a grid tap); `page 0|1`; `react preset|beats|peaks`; `fins cal port|starboard|both up|down|clear` (NL_OP_FIN_CAL to both brains: the fin sensor reading now becomes that limit; hold the fin at its limit first); `fins on|off|toggle` (WLED's fin
  LEDs follow the fins, or stay at the modelled angle); `shot`; `[eyes|wled] bump flash MS`,
  `[eyes|wled] bump black MS`, `[eyes|wled] bump preset N MS` (no prefix:
  flash and blackout go to both, preset bumps to WLED, as the preset numbers
  differ). Commands are unicast with an ack and up to 3 tries of
  60 ms.

## Radio: searching together, site survey, network moves (2026-11-09)

- **No brain heard for 12 s:** the base leads a search (it used to anchor
  channel 6, and stayed there for good once the brains came back elsewhere).
  The eyes and preview catch up with it and search with it; see
  docs/architecture.md, "Radio". The log and the details page say
  `searching (leading)`.
- **Survey page** (page 4, swipe left of the details): SURVEY takes the base
  off the network for about 6 s (13 channels x 300 ms of listening, then an
  access point scan), and lists per channel the other networks' access
  points and airtime, ours left out, and a bar for the air a network there
  would share (neighbouring channels count; `nl_survey_score`). Only 1..11
  are recommended (`BASE_MAX_CHANNEL`), and only when clearly better (a
  fifth less and more than 2% of airtime). HOLD TO MOVE (a long press) moves
  everyone 1 s later; it's enabled only while locked onto a brain and when a
  move is worth it.
- Tested on the bench 2026-11-09: the survey ran (6.0 s off the network,
  back at once); a move with brains that didn't know about moves yet: the
  base moved, gave up after 11 s and found the brains on their channel
  again in under 1 s.

## Updates without USB, and the mirror board (2026-11-02)

- **Partition table** (`partitions.csv`): two 6 MB app slots (`ota_0`,
  `ota_1`) and `otadata`; nvs where it always was, so settings survive. The
  image is ~1.4 MB. Changing to it needed one USB flash per board (done on the
  base prototype 2026-11-02; a 2 MB backup from before is in `wled/backup/base/`).
  `idf.py flash` still works as before (it resets `otadata` to boot `ota_0`);
  **don't use `app-flash`** once a board has been updated: it writes `ota_0`
  while the board may be booting `ota_1`.
- **Rollback** (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`, `main/ota.c`): a new
  image is on trial until it has run `OTA_GOOD_AFTER_S` (30 s); a restart before
  that goes back to the old one. Tested on the board: kept after 30 s; a reset
  during the trial went back.
- **The protocol** (`main/ota.h`), text lines over the USB console or the
  mirror cable: `ota begin SIZE`, `ota data OFF CRC32 BASE64` (768 bytes a line,
  zlib's CRC-32), `ota end`, `ota abort`, `ota status` (slot, build = first 16
  hex digits of the ELF's SHA-256, size, "on trial"). `tools/base_ota.py usb
  PORT` sends `build/nibbles_base.bin` this way (~100 s over USB, tested);
  `status PORT` shows the slot and build. The USB console's receive buffer is
  4 KB for these lines (at 256 bytes it dropped bytes).
- **Wi-Fi** (main board only, off unless asked): `wifi set SSID PASSWORD`
  (saved in NVS; the user types it, Claude doesn't), then `ota wifi on`, or
  **PAGE held 5 s while the keys are locked** (locked, PAGE does nothing else;
  again turns it off). It joins on the radio's channel, so ESP-NOW keeps
  working, and serves `GET /` (version) and `POST /ota` (the raw .bin:
  `tools/base_ota.py wifi IP`, or `curl --data-binary @nibbles_base.bin
  http://IP/ota`). The page-1 event line shows `Update: http://IP/`. The netif
  is made before the radio starts Wi-Fi (`ota_wifi_init`). **Not yet tested
  against a real network** (needs the password); the key combo and the "no
  network saved" path are.
- **The code moved to `shared/nibbles_ota`** (2026-11-17) so the monitors and the
  eyes share it. **`ota remote on [eyes|monitors|all] [MIN]`** broadcasts
  `NL_MSG_OTA_WIFI` three times with the network saved by `wifi set` (sent in the
  clear: ESP-NOW broadcasts can't be encrypted); the boards named join it for MIN
  minutes (default 15, longer while an update is being written) and broadcast
  `NL_MSG_OTA_STATE` once a second (`main/ota_peers.c`). `ota remote` lists them
  (name, `http://IP/`, build, progress, the port eye's state); a board getting an
  address shows on the event line. `ota remote off [...]` sends them off again.
  **Tested 2026-11-17** with the Right Brain's `Nibbles` (password `wled1234`, WLED's
  default; "wireless" was refused with a handshake timeout): the eyes and both
  monitors joined in about a second (4.3.2.3-5) and the radio kept working;
  uploads took 3.7 s (eyes) and 6.9 s (a monitor), each kept after 30 s.
  **The brain's access point takes only a few clients** (the Mac couldn't join,
  CoreWLAN error -3912 / tmpErr, until the monitors left): ask for one group at a
  time (`ota remote on eyes`, then `monitors`). A network a board can't join is
  retried at 2 s doubling to 30 s, since each try scans every channel and takes
  the board off ESP-NOW's for a moment.
  **No USB needed** (2026-11-17): the base also takes `ota remote ...`, `ota wifi
  on|off` and `ota wifi` (its address) over the radio from a brain
  (`NL_MSG_CONSOLE`, `tools/base_ask.py`), so a computer on `Nibbles` drives the
  whole update. Tested: the base updated itself over Wi-Fi (11.3 s, kept). **Its
  Bluetooth goes off when its Wi-Fi goes on**: with the clock window still
  advertising, it joined but answered nothing. Steps: the `update-boards` skill.
  **Unattended** (2026-11-17, for a computer with no other internet than the
  Wi-Fi it moves to Nibbles): `tools/ota_push.py --detach eyes monitors base`
  does the whole round and checks every board kept its image; tested on all five
  boards in 4 min.
- **Mirror board** (`main/mirror_link.c`): a second 3.49 up the pole runs the
  same firmware with `role mirror` (USB serial; `role base` back; saved in
  NVS). It runs only the screen and the cable: no radio, power board, keypads
  or logger, and shows `MIRROR <build>, main heard|not heard` on the event line
  (the screen's contents aren't mirrored yet). Tested on the base prototype
  (then set back to `role base`).
  - Cable: UART1 at `MIRROR_BAUD` (1 Mbaud), 8N1, 3.3 V. Main board TX GPIO 0
    (header A1) and RX GPIO 4 (A5); mirror TX GPIO 43 (B5), RX GPIO 44 (B6);
    crossed, plus ground. **The main board needs two capacitors removed for
    1 Mbaud**: C58 (100 nF on GPIO 0, the BOOT key's) and C10 (100 nF on GPIO 4,
    the battery ADC's). Every header GPIO on the 3.49 is either used (1, 2, 3,
    5, 43, 44; 38-41 SD; 19/20 USB; 47/48 its own I2C) or has one of these.
    GPIO 0 is a strapping pin, so the mirror never transmits unasked and only
    drives the main's GPIO 4.
  - The main's end is off until `mirror link on` (then a restart; the BOOT key
    stops being "next preset"). Every 2 s it sends `hello`; the mirror answers
    `MIRROR <build> ready|busy`. If the builds differ, the main sends its own
    running image with the protocol above (once per build per boot), so one
    update to the main updates both. `mirror` shows the state, `mirror push`
    sends anyway. **The push over the cable is untested** (no second 3.49);
    the main end was switched on with nothing attached ("no mirror", no harm).

Build: `idf.py -C firmware/base build`, flash to the base board's port
(check its serial number, 28:84:85:90:93:00, first; 80:45:6B:35:D0:14 was the base from 2026-11-02 until it died on 2026-11-08, and 28:84:85:91:27:08 before that).
