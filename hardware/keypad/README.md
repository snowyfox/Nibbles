# Keypad boards

Physical buttons for the base station, replacing the 3.49's touch screen as
the main input (see `firmware/base/`). Small 2-layer boards, made by
`tools/hardware/make_keypad_kicad.py` (run with KiCad's Python; see its
header). Once a board has been edited in KiCad, edit it there, not by
rerunning the script, which overwrites the project.

Since 2026-11-08 the key boards **daisy-chain**: each has an IN (J1) and an
OUT (J2) connector and its own 5 V buffer for the LED data, so no hub is
needed. key_dual2 (2026-11-08) is key_column_chain's routed board twice over:
column B is the same layout 18 mm to the right (16 mm would overlap the copper
along the column edges), references + 100 and nets suffixed `_B` (GND shared).
Chain A's J2 to B's J101 with a short GH8 cable; set B's address with JP101
(cut to 2-3 for 0x39). key_upper and key_column_chain are routed and DRC clean apart from
silkscreen. **key_dual is not finished:** autorouted (Freerouting) and cleared
of edge-clearance errors, but KEY15's pull-up (RN4 pin 3), column A's +5V chain
to the main +5V, and GND stitching for a few back-pour islands (6 starved
thermals) are still open; DRC lists them as unconnected.

| Project | What | Size |
|---|---|---|
| `key_dual2/` | 16 keys: two complete key_column_chain columns side by side, 18 mm apart, each with its own J1/J2 (B: J101/J102); routed, DRC clean apart from silkscreen | 36 x 168.2 mm |
| `key_dual/` | 16 keys at 16 mm both ways, two TCA9534As, one J1/J2: not finished (see below) | 36 x 160.4 mm |
| `key_upper/` | 3 keys in one column, for the top of the pole's bottom segment, daisy chain | 18 x 88.2 mm |
| `key_column_chain/` | 8 keys in one column, daisy chain | 18 x 168.2 mm |
| `key_column/` | 8 keys in one column, the earlier hub version (one connector), kept as it was | 18 x 156.2 mm |
| `key_hub/` | the earlier hub: power board keypad port out to 4 hub-version boards | 48 x 24 mm |

`nibbles_keys.pretty` holds the hotswap socket footprint, shared by the key
boards. Everything else is KiCad's own library.

## How it connects

```
power board J9 (GH6) ─┐
3.49 GPIO 43 (wire) ──┴─ J1 key_dual J2 ── J1 key_upper J2 ── (J1 of a further board)
```

Every chain board has the same **JST-GH 8** pinout on J1 (in) and J2 (out),
wired straight through: 1 GND, 2 +3V3, 3 SDA, 4 SCL, 5 INT, 6 +5V, 7 LED
data (in on J1, out on J2), 8 unused. Pins 1-6 are the power board's GH6
keypad pinout (J9, J11). Plain straight-through GH8 cables link the boards;
the order of the boards sets the order of the LEDs.

- **The cable from the power board** is J9's GH6 plus one wire from the
  3.49's GPIO 43, on the first board's J1 pin 7. GPIO 43 is free (UART0 TX; the base's
  console is on USB, and the dock UART only uses GPIO 44 to receive). On the
  power board it's J6 pad B5, which isn't connected: solder the wire to that
  header pin's tail on the bottom, or to the 3.49's own hole if using J10.
  J11 stays spare.
- **Keys:** each TCA9534A (one per column; key_dual has two) reads its keys (pressed = low; 10k
  pull-ups, since the TCA9534A has none) and pulls the shared INT low on
  any change. Addresses 0x38-0x3F (A0-A2 on JP1-JP3, bridged to 0 as made;
  cut 1-2 and bridge 2-3 for a 1), clear of the power board's TCA9534 at
  0x20, the INA226 at 0x40 and the BQ25798 at 0x6B. Give every expander its
  own address: key_dual's U2 uses JP4-JP6, so cut JP4 to 2-3 for 0x39, and
  key_upper then needs e.g. 0x3A (cut its JP2 to 2-3). The I2C and INT
  pull-ups are on the power board.
- **LEDs:** SK6812MINI-E (RGB, reverse mount: soldered on the back, shining
  through a hole into the switch's LED window). Each board's 74AHCT1G125
  (U3, with a 100k pull-down on its input) buffers the data from J1 pin 7
  to 5 V, through 100 ohm into the first LED; the last LED drives J2 pin 7
  through 100 ohm. On key_dual the data runs down column A, then down
  column B, so LED order = key order. The firmware caps the total
  brightness: 19 LEDs at full white would draw over 1 A from the 5 V on the
  GH cables.

## Key boards

- **Kailh Box Navy** switches (MX pins) in **Kailh CPG151101S11 hotswap
  sockets** on the back, 16 mm pitch (key_dual: 16 mm both ways). Box housings are
  15.6 mm square, so a 14 mm plate cutout leaves a 2 mm web between keys.
  No keycaps assumed: the custom rockers press the stems directly.
- The LED sits on the switch's LED-window side, opposite the two metal
  pins (checked on a real Box Navy, 2026-10-29).
- Everything is on the back: one-sided assembly, hand-solderable (0603,
  TSSOP-16, the LED's 4 pads). The front only has the switch holes.
- M2 holes at both ends.
- Routing (laid out by hand on `key_upper`): everything on the back, GND
  pours on both sides. KEY1-3 and the LED data in run down the **back's
  right edge** (only four lanes fit past the socket pads). `key_column` is
  `key_upper`'s layout copied key by key, with the electronics 80 mm lower
  (a scratch script, 2026-11-02); its KEY4-8 lines leave each socket pad
  by a via and run down the **front's right edge** in 0.15 mm lanes
  (four of them squeeze between SW8's hole and D8's window), round the
  west end of the front +5V trace and back through vias under U1's
  KEY5-8 pins; KEY4 drops beside U1 pin 7.
- The chain parts (2026-11-08): on key_upper and key_column_chain, J2, U3
  and its resistors and capacitor sit in a strip below J1, 12 mm longer
  than before; the user's routing is kept, apart from the old LED data
  into J1 and one corner of LED_LAST (moved right to let LED_IN out of
  J1 pin 7). key_dual's per-key LED and 5 V wiring is key_column's, copied
  key by key into both columns; the rest of key_dual, and the new
  connections on the other two, were routed with Freerouting 2.1.0 and
  finished by hand (scripts in the session, not kept). key_dual is 1 mm wider
  each side than the generator first made it (36 mm), for the KEY lines along
  its edges, and two GND stitching vias near D1 and D9 moved up 0.15 mm.

## Parts (LCSC, in stock 2026-10-28)

| Part | LCSC |
|---|---|
| TCA9534APWR | C206010 |
| SK6812MINI-E | C5149201 |
| Kailh CPG151101S11 hotswap socket | C41430893 |
| JST BM08B-GHS-TBT (GH8 vertical) | C133062 |
| 10k x4 array 0603 (4D03WGJ0103T5E) | C29718 |
| SN74AHCT1G125DBVR (one per chain board) | C7484 |
| 100 nF 0603 / 10 uF 0805 / 22 uF 0805 | C14663 / C15850 / C45783 |
| 100R / 100k 0603 | C22775 / C25803 |

## Checks

ERC clean and the schematic matches the board (DRC's parity check) for all
of them. DRC is clean on the hub; on the key boards it still lists silkscreen
clipped by solder mask, which the fab trims. The board edge clearance is 0.2 mm: KiCad's own
SK6812MINI-E footprint puts its pads 0.25 mm from its cutout.
