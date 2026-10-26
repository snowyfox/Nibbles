# Charger board

The BQ25798 buck-boost charger for the internal 4S pack, as its own small
board on the [power board](../power_board/README.md)'s charger port. It is a
**KiCad project** (`kicad/nibbles_charger_board.kicad_pro`; open this one in
KiCad) and an EAGLE schematic for Fusion (`fusion/nibbles_charger_board.sch`),
generated with the power board from
`tools/hardware/make_power_board_sch.py` (see the power board's README for
the commands, where the footprints come from, and the cable pinouts).

Why a separate board (2026-10-25): it's the riskiest part of the design and
the only one that needs 4 layers. If it misbehaves, last year's USB-C
modules (plan B in docs/power.md) plug into the same port while this board
is reworked, and nothing else changes.

- **4 layers**: top parts and short connections, In1 a solid ground plane,
  In2 SW1/SW2 and power, bottom anything else.
- All 34 footprints, every pad on its net, **roughly placed** on a
  placeholder 54 x 33 mm outline (`PLACE_CHARGER` in
  `make_power_board_kicad.py`); not routed. Input from the power board on
  the left (J21), the battery output on the right (J22), the I2C cable (J23)
  and the cells' NTC (J4) on the bottom.
- Net class **Power**, 3 mm, vias 0.8/0.4 mm (DOCK_VIN, PMID, SYS, SW1,
  SW2, PACK_F; 1 oz copper): up to ~6 A in the inductor when boosting from
  a low dock. Better as short pours, as in TI's layout example; neck down
  where they meet the BQ25798's pins. Everything else 0.25 mm; ground is
  In1 plus top pours with several vias at each capacitor.
- The BQ25798 is laid out after its datasheet (section 8.4.1, Figure 8-21):
  0402s from PMID and SYS to GND right at those pins, a 10 uF beside each,
  the inductor just above (SW1/SW2 go down through vias under the IC to
  In2), the VBUS and REGN caps at the left pins, the BAT caps at the right.
- Checked: KiCad's netlist connects exactly the design's 24 nets; ERC shows
  only labels on one pin (QON, a test point, and +3V3/+5V, which the cable
  carries but this board doesn't use); DRC shows only unrouted connections
  and silkscreen crowding.

| Port | Pins | Power board end |
|---|---|---|
| J21, XT30 | 1 DOCK_VIN (+), 2 GND | J10 |
| J22, XT30 | 1 PACK_F (+), 2 GND | J11 |
| J23, JST-GH 6 | 1 GND, 2 +3V3, 3 SDA, 4 SCL, 5 INT, 6 +5V | J12 |
| J4, JST-PH 2 | 1 TS (NTC), 2 GND | 103AT NTC against the cells |

## Design
- BQ25798RQM: PROG 27.0 kΩ = 750 kHz and 4S defaults, so the inductor is
  2.2 µH (1.5 MHz needs 1 µH instead). One input straight to VBUS with no
  ACFET-RBFETs (datasheet 7.3.5.2, Figure 8-1): VAC1 and VAC2 tied to VBUS,
  ACDRV1 and ACDRV2 to GND. The power board's U9 blocks any current back
  out through the LEMO; firmware must still never enable OTG.
- **Input limit 2.0 A at power-up**: ILIM_HIZ divider R37/R38 (10k/10.7k
  from REGN, 2.59 V; VILIM = 1 V + 0.8 Ω × ILIM, 1.85-2.11 A over REGN's
  4.8-5.2 V). Lights at full white (~7 A) plus charging then stay under a
  VB99 D-Tap's 10 A with no firmware. The base can raise it by clearing
  EN_EXTILIM and writing IINDPM (datasheet 7.3.4.3); without that, the pin
  clamps the register.
- TS divider 5.23k/30.1k (datasheet:
  5.24k/30.31k for 0-60 °C with a 103AT NTC). No ship FET (the BMS protects
  the pack). SYS powers nothing: the base's 5 V comes from the power board.
- BQ25798 at 0x6B on the shared power I2C bus; pull-ups and the INT pull-up
  are on the power board.

## Parts (JLCPCB stock, 2026-10-24)

| Ref | Part | LCSC | Notes |
|---|---|---|---|
| U2 | BQ25798RQMR | C2876593 | |
| L1 | MWSA0603S-2R2MT | C408445 | 2.2 uH, 10 A sat. |
| LED1 | KT-0603R (red) | C2286 | charging (STAT) |
| C10-C24 | 10 uF 25 V 0805 | C15850 | Basic |
| C12, C16, C22 | 0.1 uF 0402 | | high-frequency decoupling at the pins (datasheet 8.4.1) |
| J21, J22 | Amass XT30PW-M | C431092 | |
| J23 | JST SM06B-GHS-TB | (match at ordering) | |
| J4 | JST S2B-PH-K-S | C173752 | NTC |

Datasheet pins: BQ25798RQM (VQFN-29), TI SLUSDV2C: STAT 1, VBUS 2-3, BTST1
4, REGN 5, D+ 6, D- 7, VAC2 8, VAC1 9, ACDRV2 10, ACDRV1 11, QON 12, CE 13,
SCL 14, SDA 15, TS 16, ILIM_HIZ 17, BATP 18, BTST2 19, PROG 20, INT 21, BAT
22-23, SDRV 24, SYS 25, SW2 26, GND 27, SW1 28, PMID 29.

## To check before layout
- Compare the sheet with datasheet Figure 8-1 (single input, no input FETs,
  no ship FET), including its optional input snubber (recommended when
  hot-plugging adapters above 15 V: the dock's long cable).
- XT30 polarity, as on the power board (+ on the round pad 2).
- Mounting holes, and the outline against where it sits in the enclosure.
