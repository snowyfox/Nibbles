# Pole-base power board

The power board in [docs/power.md](../../docs/power.md), as a **KiCad
project** (`kicad/nibbles_power_board.kicad_pro`, KiCad 10; open this one in
KiCad) and as an EAGLE schematic for Fusion (`fusion/nibbles_power_board.sch`,
which KiCad can't open). The charger is a separate
board that plugs into this one's charger port:
[charger_board](../charger_board/README.md). Both boards are generated from
one design description, `tools/hardware/make_power_board_sch.py`:

    python3 tools/hardware/make_power_board_sch.py           # EAGLE/Fusion schematics, both boards
    KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
    $KPY tools/hardware/make_power_board_kicad.py            # KiCad projects, both boards (needs KiCad's Python)

KiCad is the one to continue in: its files are text, so they can be checked
and edited outside the GUI (`kicad-cli` runs the ERC and DRC and makes the
JLCPCB outputs). The KiCad project has:
- the schematic: a root sheet and the three sheets below, every net on
  global labels (plain net names, the same names as on the charger board),
  intentionally open pins flagged no-connect;
- the board: **2 layers** (top parts and connections, a solid ground plane
  on the bottom), all 65 footprints with every pad on its net, **roughly
  placed** (`PLACE_POWER` in `make_power_board_kicad.py`) on a placeholder
  100 x 70 mm outline; not routed. Power runs left to right:
  - left edge: everything from the LEMO (dock power J2, dock signals J7)
    and the pack (J3), each XT30's mating side on the edge; the LEMO's ideal
    diode (U9/Q9) right after J2;
  - top edge: the dock LED header (J8) and the charger port (J10 input, J11
    battery, J12 I2C);
  - middle: the dock's and the pack's ideal diodes into LIGHT_BUS;
  - right: the eFuse and its settings, the bulk caps (C32, C33) on its
    output, the shunt and INA226, the lights' XT30
    (J5) on the right edge; the 5 V buck, I/O expander, base connector (J6,
    right edge) and keypad port (J9, bottom edge) bottom right;
- project rules for JLCPCB's standard process: 0.15 mm default clearance
  (TI's TPS25982 land pattern has 0.165 mm gaps to its thermal pads),
  0.25 mm hole-to-copper.
Checked: KiCad's netlist connects exactly the design's 40 nets; ERC is
clean; DRC shows only unrouted connections and silkscreen crowding (no
courtyard overlaps).

**Regenerating overwrites the KiCad files**, the board placement included.
Once layout work starts in KiCad, make further changes there (or ask for
them to be made in a way that keeps the layout).

**Status: first draft, not reviewed.** Pin functions were taken from each
part's datasheet, but check every symbol against its datasheet before layout,
and compare each block with the datasheet's application circuit.

## Boards and ports (split 2026-10-25)

The charger was split off so it can be swapped on its own: if the BQ25798
board misbehaves, last year's USB-C modules (plan B in docs/power.md) take
its place on the same port. The BQ25798 was also the only part that needed
4 layers, so this board went back to 2.

| Port | Here | Pins | Other end |
|---|---|---|---|
| Charger input | J10, XT30 | 1 DOCK_VIN (+), 2 GND | charger board J21, or plan B's 60 W USB-C board |
| Charger battery | J11, XT30 | 1 PACK_F (+, after the pack fuse), 2 GND | charger board J22, or plan B's 140 W charger |
| Charger I2C | J12, JST-GH 6 | 1 GND, 2 +3V3, 3 SDA, 4 SCL, 5 INT, 6 +5V | charger board J23 (unused in plan B) |
| Keypad | J9, JST-GH 6 | same as J12 | the keypad's I2C expander (address 0x21-0x27; 0x20 is U8) |
| Base | J6, JST-GH 8 | 1 +5V, 2 GND, 3 +3V3 in, 4 SDA, 5 SCL, 6 INT, 7 dock UART, 8 dock detect | Waveshare 3.49 V2 |
| Dock LED | J8, JST-PH 2 | 1 LED anode (via R36 7.5k from DOCK_VIN), 2 GND | a panel LED with leads, no resistor |

`CHARGER_IN`, `CHARGER_BAT` and `I2C_PORT` in the generator define these
pinouts once, for both ends. Pull-ups for the shared I2C bus and INT are on
this board. The JST-GH cables run pin 1 to pin 1 (some pre-made GH cables
are reversed: check before plugging in).

## How it is drawn
- **One power input, the LEMO** (dock, or the USB-C to LEMO charging
  dongle at home); no USB-C on this board. Refs J1, U1, D1, R1, C1, Q1-Q4
  and R34 are unused: they were the USB-C input, its PD sink and the
  charger's input FETs, removed 2026-10-25. U2's parts (C10-C28, R10-R15,
  L1, LED1) and J4 are on the charger board.
- **Two ideal diodes on the dock** (2026-10-25): U9/Q9 right after the LEMO,
  so no current ever leaves the shark through it (the pack, the charger, or
  plan B's modules, whose step-down board conducts backwards, feeding the
  dock); U3/Q5 the dock's OR diode into LIGHT_BUS, without which the pack
  would feed the charger's input through LIGHT_BUS.
- The 5 V buck runs from LIGHT_BUS (dock or pack), not from the charger, so
  the base works with no charger board fitted.
- **Hot plugging** (2026-10-25): the LEMO is plugged and unplugged live, with
  the dock always on (a VB99's D-Tap, no switch). The bulk caps C32/C33 sit
  behind the eFuse (EFUSE_OUT), which ramps them up, so plugging a live dock
  into a switched-off shark charges only ~30 uF of ceramics through the
  LEMO's contacts. They still hold up the converter's input while the ideal
  diodes switch. Unplugging always breaks the lights' current (all LEMO
  contacts part together); at ~15 V that arc is small, as last year showed.
- Three sheets: 1 input (dock connector, the LEMO's ideal diode, dock LED),
  2 lighting path (pack input, ideal diodes, eFuse, INA226), 3 logic (5 V
  buck, I/O expander, base connector, charger and keypad ports). Each sheet
  starts with notes on its design choices.
- Each chip's (and connector's) own resistors, capacitors and diodes sit
  below it, wired to the pin they belong to; their other end, and every
  signal that goes to another part of the board, ends in a net label.
  Labels with the same name are one net, on any sheet (`show NETNAME` in
  Fusion's command line highlights a whole net). FETs and parts that
  belong to no chip are in rows below, with a label on each pin.
- **Every part has a footprint** and, where it matters, a manufacturer part
  number and LCSC number (part attributes `MPN` and `LCSC`, for JLCPCB's BOM
  tool), chosen from JLCPCB's stock on 2026-10-24. Plain resistors and
  capacitors have no fixed part number: JLCPCB matches them by value and
  size (0603 unless noted; 10 uF 25 V and 22 uF 10 V in 0805, 10 uF 35 V in
  1206).
- Where the footprints come from (`hardware/libraries/`, shared by both
  boards):
  - the TI chips: TI's symbols and land patterns from Ultra Librarian
    ("Eagle6 Library" downloads, not committed: their terms may not allow
    republishing), converted by `tools/hardware/eagle_scr.py`; TI's own land
    pattern ("-MFG") where there is one, else the nominal one. `LIBPARTS` in
    the generator maps this design's pin names to TI's (CE -> CE_N, VBUS ->
    VBUS and VBUS_2, the eFuse's eight OUT pins and its thermal pads: pad 25
    = IN, pad 26 = GND);
  - everything else: KiCad's official footprint library
    (`libraries/kicad/`, CC-BY-SA 4.0 with an exception for designs that use
    it, see its LICENSE.md), converted by `tools/hardware/kicad_mod.py`;
    `PADS` in the generator maps symbol pins to pads where they differ;
  - the CH224K's ESSOP-10 (in `kicad/nibbles.pretty`, not used on either
    board; kept for the USB-C to LEMO dongle): drawn in the generator from
    WCH's package drawing (datasheet section 8). EasyEDA has one too, but its
    library data carries a copyright notice aimed at AI tools whose terms
    weren't fully readable, so it wasn't used.

## Parts (JLCPCB stock, 2026-10-24)

| Ref | Part | LCSC | Notes |
|---|---|---|---|
| U3, U4, U9 | LM74700QDBVRQ1 | C2941042 | dock OR, pack OR, LEMO (no backfeed) |
| U5 | TPS259827ONRGET | C2155765 | the small-reel version of ONRGER |
| U6 | **INA226AIDGSR** | C49851 | the INA228 had no stock; the INA226 has the same package and pins (16-bit, 36 V; enough here) and is what the test modules use |
| U7 | TPS62933DRLR | C3200405 | |
| U8 | TCA9534PWR | C783615 | |
| D2 | SMBJ18A | C10212 | dock input TVS: clamps under the 30 V FETs; conducts from about 20 V, so the dongle asks for 15 V (or change this for a 20 V dongle) |
| D3, D4 | TPD1E10B06DPYR | C48260 | footprint: KiCad D_0402 (the DPY package is 1.0 x 0.6 mm); compare with TI's land pattern |
| Q5, Q6, Q9 | NTMFS4C022NT1G (SO-8FL) | C179623 | 30 V, 2.4 mOhm; Q9 carries lights plus charging |
| Q7, Q8 | 2N7002 | C8545 | Basic |
| L2 | MWSA0603S-6R8MT | C408449 | 6.8 uH, 6 A |
| F1 | CC12H15A-TR (1206) | C2443310 | 15 A: close to its limit at ~11 A worst case, check its derating |
| R29 | LR2512-23R002F4 | C154685 | 2 mOhm 3 W shunt |
| C32, C33 | 220 uF 25 V polymer, SMD 6.3 x 9 mm | C2981459 | |
| J2, J3, J5, J10, J11 | Amass XT30PW-M | C431092 | dock power (from the LEMO), pack, converter, charger port |
| J6 | JST SM08B-GHS-TB | C265111 | to the base board |
| J9, J12 | JST SM06B-GHS-TB | (match at ordering) | keypad and charger I2C ports |
| J7 | JST S3B-PH-K-S | C157929 | dock data, detect, ground (from the LEMO) |
| J8 | JST S2B-PH-K-S | C173752 | dock LED |

The dock's LEMO 1K.308 sits in the enclosure wall and is wired to J2
(power) and J7 (signals); it isn't on the board. The pack's main power
switch (a latching button at the bottom of the pole, as last year) sits in
the pack's lead between the BMS and J3.

## Parts and datasheet pins

| Ref | Part | Datasheet | Pins used here (name = package pin) |
|---|---|---|---|
| U3, U4, U9 | LM74700-Q1 (SOT-23-6, DBV), ideal diode | TI SNOSD17G | VCAP 1, GND 2, EN 3, CATHODE 4, GATE 5, ANODE 6 |
| U5 | TPS259827ONRGE (QFN-24), eFuse | TI SLVSEI3D | IN 1-3, 16 and pad 1; GND 4, 5, 14 and pad 2; EN/UVLO 6; ITIMER 7; ILIM 8; IMON 9; RETRY_DLY 10; NRETRY 11; LDSTRT 12; PG 13; dVdt 15 (DVDT here); OUT 17-24 |
| U6 | INA226 (fitted; INA228 pin-compatible) (VSSOP-10), power monitor | TI SLYS021A (INA228) | A1 1, A0 2, ALERT 3, SDA 4, SCL 5, VS 6, GND 7, VBUS 8, IN- 9, IN+ 10 |
| U7 | TPS62933DRLR (SOT-583), 5 V buck | TI SLUSEA4D | RT 1, EN 2, VIN 3, GND 4, SW 5, BST 6, SS/PG 7 (SS here), FB 8 |
| U8 | TCA9534PWR (TSSOP-16), I/O expander | TI | A0 1, A1 2, A2 3, P0-P3 4-7, GND 8, P4-P7 9-12, INT 13, SCL 14, SDA 15, VCC 16 |

Why these: see docs/power.md. Choices made for this draft:
- **TPS259827ONRGE**: the variant without overvoltage lockout (the 16.9 V
  variants would trip on a full 16.8 V pack or dock) and with circuit-breaker
  response. ILIM 130 Ω ≈ 11.3 A (datasheet: 300 Ω 5.0 A, 182 Ω 8.1 A,
  100 Ω 14.7 A). EN/UVLO divider 86.6k/10k: on at about 11.6 V, off at
  about 10.6 V (thresholds 1.2 V rising, 1.1 V falling).
- **TCA9534** on the power I2C bus (0x20) drives LIGHT_OFF and PACK_OFF
  through 2N7002s and reads the eFuse's power-good, so the
  base board still needs only 5 GPIOs. Both outputs default to off-the-FET,
  i.e. lights on and pack diode on, so everything works without the base.
  P2 and P4-P7 are spare.
- One shared open-drain interrupt (PWR_INT) for everything on the bus: the
  BQ25798, INA226, TCA9534 and the keypad's expander.
- No fuel gauge (off-the-shelf 4S BMS on the cells): the INA226 counts the
  lights' charge and the BQ25798's ADC sees charging; firmware estimates the
  state of charge from those and the pack voltage.
- **Dock LED** R36 7.5k: about 1.9 mA at 16.8 V, 1.2 mA at 11 V (bright
  enough for an indicator); 32 mW in R36 at most.

## To check before layout
- XT30 polarity: KiCad's XT30PW-M footprint marks its round pad 2 "+" and
  the square pad 1 "-", so every XT30 here puts + on pad 2 (the symbols'
  pin 1 is +). Check that against the connector's own marking and the cables.
- The TPD1E10B06 footprint (KiCad 0402) against TI's land pattern.
- The dongle's voltage (docs/power.md, open decisions): 15 V fits D2 as it
  is; 20 V needs a higher TVS.
- TPS25982: ITIMER, RETRY_DLY and dVdt capacitors (marked TBD) for the
  converter's input capacitance and the flash-bump current steps; the IMON
  resistor (or leave IMON for a test point, per the datasheet).
- TPS62933: inductor (6.8 µH here) against the datasheet's table for 5 V
  out at 500 kHz; the Waveshare board's peak current.
- MOSFETs: Q5/Q6 carry the lights (about 8 A, more on flashes), Q9 the
  lights plus charging (about 11 A worst case): low RDS(on), 30 V.
- Copper: the ~11 A path on wide pours on both layers joined by many vias;
  2 oz copper at JLCPCB.
- The outline: 100 x 70 mm is a placeholder (the board now has spare room);
  size it to the enclosure and move the connectors to where the LEMO, pack
  and cables actually are. Mounting holes.
- The JST-GH 6 (SM06B-GHS-TB) LCSC number, at ordering.
- Which Waveshare V2 pins the base connector (J6) goes to (power.md TODO).
