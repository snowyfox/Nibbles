# Keypad boards

Physical buttons for the base station, replacing the 3.49's touch screen as
the main input (see `firmware/base/`). Three small 2-layer boards, made by
`tools/hardware/make_keypad_kicad.py` (run with KiCad's Python; see its
header). Every part is placed; **nothing is routed yet**. Once a board has
been edited in KiCad, edit it there, not by rerunning the script, which
overwrites the project.

| Project | What | Size |
|---|---|---|
| `key_column/` | 8 keys in one column, RGB LED under each, TCA9534A | 18 x 156 mm |
| `key_upper/` | the same with 3 keys, for the top of the pole's bottom segment | 18 x 76 mm |
| `key_hub/` | plugs into the power board's keypad port, fans out to 4 key boards | 48 x 24 mm |

`nibbles_keys.pretty` holds the hotswap socket footprint, shared by the key
boards. Everything else is KiCad's own library.

## How it connects

```
power board J9 (GH6) ─┐
3.49 GPIO 43 (wire) ──┴─ hub J1 (GH8) ── J2 ── key column 1
                                       ├ J3 ── key column 2
                                       ├ J4 ── (spare / another column)
                                       └ J5 ── upper keypad (60-80 cm cable)
```

Every key board and hub port uses the same **JST-GH 8** pinout:
1 GND, 2 +3V3, 3 SDA, 4 SCL, 5 INT, 6 +5V, 7 LED data in, 8 LED data out.
Pins 1-6 are the power board's GH6 keypad pinout (J9, J11).

- **The cable from the power board** is J9's GH6 plus one wire from the
  3.49's GPIO 43, on hub J1 pin 7. GPIO 43 is free (UART0 TX; the base's
  console is on USB, and the dock UART only uses GPIO 44 to receive). On the
  power board it's J6 pad B5, which isn't connected: solder the wire to that
  header pin's tail on the bottom, or to the 3.49's own hole if using J10.
  J11 stays spare.
- **Keys:** each key board's TCA9534A reads its keys (pressed = low; 10k
  pull-ups, since the TCA9534A has none) and pulls the shared INT low on
  any change. Addresses 0x38-0x3F (A0-A2 on JP1-JP3, bridged to 0 as made;
  cut 1-2 and bridge 2-3 for a 1), clear of the power board's TCA9534 at
  0x20, the INA226 at 0x40 and the BQ25798 at 0x6B. Give every board its
  own address. The I2C and INT pull-ups are on the power board.
- **LEDs:** SK6812MINI-E (RGB, reverse mount: soldered on the back, shining
  through a hole into the switch's LED window). The hub's 74AHCT1G125
  buffers GPIO 43 to 5 V and the data runs J2 in, through that board's
  LEDs, back out on pin 8, into J3, and so on. Fill the ports in order, or
  bridge a port's JP to skip it. Each key board's last LED drives the cable
  back through 100 ohm. The firmware caps the total brightness: 19 LEDs at
  full white would draw over 1 A from the 5 V on the GH cables.

## Key boards

- **Kailh Box Navy** switches (MX pins) in **Kailh CPG151101S11 hotswap
  sockets** on the back, 16 mm pitch in a single column. Box housings are
  15.6 mm square, so a 14 mm plate cutout leaves a 2 mm web between keys.
  No keycaps assumed: the custom rockers press the stems directly.
- The LED sits on the switch's LED-window side (opposite the pins, as on
  Cherry MX). **Check that against a real Box Navy before ordering**; if
  the window is on the other side, rotate the switch footprints 180 degrees.
- Everything is on the back: one-sided assembly, hand-solderable (0603,
  TSSOP-16, the LED's 4 pads). The front only has the switch holes.
- M2 holes at both ends.
- Routing plan (the switch pads are set up for it): key lines from each
  socket's right pad down the **front's right edge** (8 lanes at 0.15/0.15
  mm fit between the switch pegs and the edge); +5V down the **front's
  left edge**, with a via to each LED; GND pours on both sides; the LED
  chain on the back past each socket.

## Parts (LCSC, in stock 2026-10-28)

| Part | LCSC |
|---|---|
| TCA9534APWR | C206010 |
| SK6812MINI-E | C5149201 |
| Kailh CPG151101S11 hotswap socket | C41430893 |
| JST BM08B-GHS-TBT (GH8 vertical) | C133062 |
| 10k x4 array 0603 (4D03WGJ0103T5E) | C29718 |
| SN74AHCT1G125DBVR (hub) | C7484 |
| 100 nF 0603 / 10 uF 0805 / 22 uF 0805 | C14663 / C15850 / C45783 |
| 100R / 100k 0603 | C22775 / C25803 |

## Checks

ERC clean and the schematic matches the board (DRC's parity check) for all
three. DRC also reports the unrouted connections, silkscreen overlaps,
a starved thermal and a small isolated pour, which go once it's routed and
the silk is tidied. The board edge clearance is 0.2 mm: KiCad's own
SK6812MINI-E footprint puts its pads 0.25 mm from its cutout.
