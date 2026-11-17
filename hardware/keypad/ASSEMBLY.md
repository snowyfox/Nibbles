# Building the keypad boards

For the JLCPCB boards delivered 2026-11-16 (ordered 2026-11-09, with
stencils): **key_column_chain** (8 keys, J1 in / J2 out), **key_upper**
(3 keys, chain), **key_column** (8 keys, hub version) and **key_hub**. The
parts are from the combined LCSC order of 2026-11-01 (five boards' worth).
Build one key board, test it, then the rest.

**Placement pages:** one per board in `assembly/` (made by
`tools/hardware/keypad_assembly_guide.py` from the KiCad boards, the same page
as the power board's): `key_column_chain.html`, `key_upper.html`,
`key_column.html`, `key_hub.html`. One step per bag, each with the bag to take,
what the part does, how to recognise and orient it, and a map with that step's
parts highlighted (yellow dot = pin 1); on the long boards a close-up of the
step's area sits beside the whole-board map. Arrow keys move between steps,
space ticks one; ticks are remembered in that browser, per board.

## 1. Which boards, which addresses

The base finds each key board by its I2C address and lights the LEDs in chain
order (`KEYPAD_BOARDS` in `firmware/base/main/config.h`). The plan as of
2026-11-16 is three 8-key chain boards and no key_upper (more keys may come
later: give each new board the next free address):

| Order on the chain | Board | Keys | Address | Jumpers |
|---|---|---|---|---|
| 1 (J1 from the power board) | key_column_chain | NUM1-NUM8 | 0x38 | as made |
| 2 | key_column_chain | ALL, BODY, EYES, FINS, PAGE, LATCH, BLACKOUT, FIN SENSOR | 0x39 | JP1 (A0) to 2-3 |
| 3 | key_column_chain | TAKE, FLASH, UNLOCK, PALETTE, LAYER, MONITORS, DETAIL, (free) | 0x3A | JP2 (A1) to 2-3 |
| (later) | another key board | | 0x3B | JP1 and JP2 to 2-3 |

The key names are placeholders: the whole assignment will change. The
default is one table (`KEYPAD_BOARDS` in `config.h`), and the base's `keymap`
serial command, or the brains' `/keys` page from a phone (WLED's Info
panel, Nibbles pages, Keys), changes it without new firmware (see `firmware/base/CLAUDE.md`),
so the boards and jumpers don't change. An optional mirror panel (NeoPixel FeatherWing) goes on the last
board's J2: pin 7 to its data in, pin 6 to its 5 V, pin 1 to GND (see
`KEYPAD_PANEL_*` in `config.h`).

Plain straight-through GH8 cables link J2 of one board to J1 of the next.
Two boards with the same address both answer and the keys mix up: set the
jumpers before the board goes into the box, and write the address on the
board. A hub-version key_column is 0x38 as made too. Hub-version boards can skip
the hub on a harness (pin 8 of one board to pin 7 of the next; see
`README.md`, "Hub-version boards without the hub").

The hub is only needed with hub-version key_columns. A chain board on a hub
port doesn't send the LED data back, so it must be on the last port used,
with the later ports skipped (hub JP1-JP4).

## 2. Sorting the parts

All the keypad bags are from the combined order (`hardware/fab/combined-LCSC-order.csv`):

| Bag | Key boards | Hub |
|---|---|---|
| C206010 TCA9534**A**PWR | U1 | |
| C7484 SN74AHCT1G125 | U3 (chain boards) | U1 |
| C29718 10k x4 array | RN1, RN2 | |
| C14663 100 nF 0603 (shared with the power board) | C1, C4, C11-C18 | C1, C4 |
| C15850 10 uF 0805 (shared) | C2 | |
| C45783 22 uF 0805 (shared) | | C2, C3 |
| C22775 100R 0603 | R1, R3 | R2 |
| C25803 100k 0603 (shared) | R2 (chain boards) | R1 |
| C133062 GH8 vertical (shared) | J1, J2 | J1-J5 |
| C41430893 Kailh hotswap socket | SW1-SW8 (key_upper SW1-SW3) | |
| C5149201 SK6812MINI-E | D1-D8 (key_upper D1-D3) | |

Write each step number on its bag (the page's numbering; the same bag set
works for every key board). Capacitors carry no marking: one dropped on the
mat goes in the bin. The SK6812MINI-E LEDs are moisture-sensitive (the
datasheet allows 24 h out of the sealed bag at under 30 °C / 60 % RH, MSL
5a): keep their bag sealed until their step, and reseal it with the
desiccant afterwards.

## 3. Key boards: the back, stencil and hot plate

Everything on a key board is on the **back** (the side with the connectors);
the front only has the switch holes and the LED windows. The
stencil is a bottom stencil.

**Paste.** Sn63/Pb37 (Chip Quik NC191AX50 or the Sopagre), as for the power
board. Tape the board back up into a frame of spare boards, line the
stencil up by holding it to the light, one pass with the squeegee. All the
pads get paste, the LED pads too: the LEDs go on by iron after the hot
plate (step 10), and the paste tins their pads for that.

**Place** steps 1-9 following the page: U1, U3, the resistor arrays, the
capacitors and resistors, J1/J2, then the hotswap sockets last (their two
plastic bumps drop into the two holes, so they only fit one way). Leave the
LED pads empty.

**Reflow** front side down on the hot plate: the front is bare, so the board
lies flat. A 2-layer board heats faster than the power board: soak with the
plate at about 150 °C for 60-90 s, then about 230-235 °C until every joint
is shiny and the sockets' tabs have wetted, then 20 s more. The hotswap
sockets are SMD parts made for reflow. key_column_chain is 168 mm long
(key_column 156 mm, key_upper 88 mm): if the plate is shorter than the
board, reflow the connector end (U1, U3, J1/J2 and their parts, all within
the last 35 mm) on the plate, and do the rest by hot air with the plate
under it at about 120 °C as a preheater, one key at a time, or by iron.

**Inspect** U1's legs (0.65 mm pitch) for bridges, and look along each
socket: both tabs soldered, the body flat on the board.

## 4. Key boards: LEDs and address, by iron

**LEDs (step 10).** SK6812MINI-E, reverse mount: the LED goes in from the
back **lens down**, its body in the board's cutout, so the lens looks out of
the front window; the four legs rest on the tinned pads. **Orientation:** the
body's cut (chamfered) corner and the leg with a notch in it are GND, pad 1,
the yellow dot on the map (the datasheet's mechanical drawing; KiCad's
footprint marks the same corner). Flux, iron at about 300 °C, one leg first,
check the LED sits flat and square in its window, then the other three. The
datasheet's limit is 215 °C on the body for leaded solder, under 10 s near
the peak: 2-3 s per leg at the iron is well inside it, but don't hold the
iron on the body or blow hot air on the lens.

**Address (step 11)** only on the boards that need it (table above): cut the
thin trace between pads 1 and 2 of the jumper with a sharp knife, check with
the meter that they no longer connect, then bridge pads 2 and 3 with solder.

Clean the flux around U1 (IPA and a brush). Then press the Box Navy
switches in from the front: the two metal pins go into the socket, the
LED window over the LED (it's on the side opposite the pins).

## 5. Hub: the top, stencil and hot plate

Everything is on the top, with the top stencil: U1, C1/C4, R1, R2, C2/C3,
J1-J5, then the hot plate as above. J4 and J5 face the other way from
J1-J3. JP1-JP4 are open as made: bridge one with solder for every port with
nothing on it (JP1 skips J2, JP2 J3, JP3 J4, JP4 J5), or the LED data stops
there.

## 6. Checks before power (meter, nothing connected)

On J1 (pin 1 is GND; the page's map labels J1):

- +5V (pin 6) and +3V3 (pin 2) to GND: not a short (well above 100 ohms
  once the capacitors have charged).
- SDA (3), SCL (4) and INT (5): not shorted to GND, to each other or to
  +3V3. The 10k pull-ups are on the key lines, not on these: these should
  read open on a board by itself.
- On key_column_chain and key_upper, J1 pin *n* to J2 pin *n* for pins 1-6:
  connected (straight through).

## 7. First power-up

Connect the board to the power board's keypad port (J9, plus the LED data
wire from the base's key LED pin: GPIO 43 on the 3.49, GPIO 14 on the 1.91;
see `README.md`; its "Wire colours" table gives each pin's colour on every
GH cable), with the base running. Its once-a-second status log ends
in `keypads N`: N goes up by one for each board found. Then:

- Press each switch: the base's log prints `key NAME down` and `key NAME up`;
  check each is the key the table above gives it (`keys` on the USB serial
  lists the names).
- The LEDs light in the keys' colours, at the low firmware level
  (`KEYPAD_LED_LEVEL`). If none light, check the data path: J1 pin 7, U3, R1
  and D1's orientation. If they stop part way, check the first dark LED's
  orientation and its legs.
- Add the next board on J2 and check `keypads` counts it.
