# Building and bringing up the power + charger board

For the first hand-built board (JLCPCB bare board, top stencil, LCSC parts).
Build one, bring it up, then build the rest.

**Placement page:** open `assembly/placement.html` in a browser (made by
`tools/hardware/assembly_guide.py` from the KiCad board). It has one step per
bag, in the order below, each with the bag to take, what the part is and does,
how to recognise and orient it, and a map of the board with that step's parts
highlighted (yellow dot = pin 1, cathode or +). Arrow keys move between steps,
space ticks one; ticks are remembered in that browser. `placement.html#16`
opens step 16. The step numbers in this file are the page's.

## 1. Sorting the parts

The LCSC order (2026-11-01, `hardware/fab/combined-LCSC-order.csv`, 58
lines) is for all four boards, five of each. Each LCSC bag has a printed label
with the **LCSC part number (C + digits)**, the maker's part number, the
package and the quantity. The C-number is what to go by.

### Check the order in

1. Lay the bags out on the ESD mat, wrist strap on.
2. Go down `combined-LCSC-order.csv` (or the packing list) and find each
   C-number. One line arrives under a different number than the board files
   say: **C77386** (1 uF 50 V, for C49/C50; LCSC had no C15849). Note anything
   missing or short.
3. Put the three side-entry JST-GH options (C133065, C189893, C265111) aside:
   they aren't used on any board yet.

### Split by board

| Pile | C-numbers |
|---|---|
| **Keypad boards only** | C206010 TCA9534**A**PWR (not the power board's TCA9534PWR), C7484 SN74AHCT1G125, C5149201 SK6812MINI-E LEDs, C22775 100R 0603, C29718 10k x4 array, C41430893 hotswap sockets |
| **Shared: power board and keypads** | C14663 0.1 uF 0603 (all four boards), C15850 10 uF 0805 (power, key column, upper keypad), C45783 22 uF 0805 (power, hub), C25803 100k 0603 (power, hub), C133062 GH8 connector (all four) |
| **Power board only** | everything else (44 bags) |
| **Not from LCSC, for this board** | the SMBJ22A TVS (Amazon kit) for D2, the solder paste, the NTCs (for the pack cable, not the board) |
| **Don't use on this board** | C10212 SMBJ18A: replaced by the SMBJ22A (the dongle gives 20 V) |

### Set out the power board's bags

4. **Write the step number on each power-board bag** with a marker (the
   step list is below and on the page): "1" on the BQ25798, "14" on the
   0.1 uF 0603, and so on. Two bags serve two steps: C15850 (steps 32 and
   46) and C1622 (steps 16 and 47). Shared bags get the step number too.
5. Stand the bags in step order in a box or tray. Leave them **closed** until
   their step: open one bag at a time, take out what the step needs (plus one
   spare for the small passives), close it again.
6. **Capacitors carry no marking**, so a capacitor out of its bag can't be
   identified. If one falls on the mat, throw it away rather than guess.
   Resistors may be printed with a code but don't rely on it; go by the bag.
7. The chips (BQ25798, TPS259827, INA226, TCA9534, TPS62933, LM74700) may
   come in sealed moisture-barrier bags with desiccant. Keep those sealed
   until steps 1-6 and reseal the rest afterwards (a zip bag with the
   desiccant is fine), so they don't take on moisture before the hot plate.
8. For tiny parts (0402, D3/D4, U7), tip the part onto a small dish or a
   piece of tape, sticky side up, rather than reaching into the bag.

## 2. Before starting

- **Check the land patterns** against the parts in hand: L1/L2 (Chilisin
  MHCB06030 on the Sunlord footprint) and D3/D4 (TPD1E10B06, 1.0 x 0.6 mm).
- **XT30 polarity:** every XT30 has + on pad 2 (round), - on pad 1 (square).
  Check against the connector's moulded marks and your cables before soldering.
- **Choose J6 or J10** for the 3.49: J6 (stacking headers) if the board
  stacks under the 3.49 on H1/H2, J10 (GH8 cable) otherwise. Same nets.
- **Paste:** Sn63/Pb37, type 4 (20-38 µm), no-clean, for the top: Sopagre
  MN-1002 (used for the first board, 2026-11-06, while the Chip Quik
  NC191AX50 was delayed) or the NC191AX50; both melt at 183 °C, same
  profile. Out of the fridge 4 h before use (if it was refrigerated), stir
  it for a minute or two. Stencil life (NC191AX50 datasheet): over 4 h at
  50-70 % humidity, over 8 h in drier air; reflow within that. Keep the low-temp SnBi paste off this
  board: bismuth and lead make a joint that melts near 96 °C. The bottom is
  done with an iron and leaded wire.
- Kit: flux, tweezers, magnifier or microscope, meter, the programmable
  current-limited bench supply, IPA and a brush.
- ESD: wrist strap on whenever the chips are out of their bags or the board
  is in hand. The hot plate and hot air go on a heat-proof pad (silicone or
  ceramic), not on the ESD mat: most mats are vinyl or rubber and are damaged
  by heat.
- Bench supply: set its over-voltage protection (OVP) to about 1 V above each
  test voltage, and its current limit before connecting the board.

## 3. Top side: paste, place, hot plate

**Paste.** Tape the board into a frame of spare boards of the same thickness
so it can't move. Lay the stencil on, line its openings up with the pads
(hold it to the light), tape one edge as a hinge. Put a bead of paste across
one end and pull it over in one firm pass with the squeegee (or an old card)
at about 45 degrees. Lift the stencil straight up. Every pad should have a
neat grey block; look hard at U2 and U5 (the two square chips). If anything
smeared or bridged, wipe the board with IPA and print again.

**Place**, following the page. Chips go first, while the board is clear and
the paste fresh; then small parts to large, so the tall ones aren't in the way
of the tweezers. Set each part straight down onto its paste, matching the
yellow dot; a slight misalignment corrects itself when the solder melts, so
don't push parts around or press them in.

| Step | Parts | What | Package, how to orient |
|---|---|---|---|
| 1 | U2 | BQ25798 battery charger | QFN-29 4x4 mm, pads underneath; pin-1 dot |
| 2 | U5 | TPS259827 eFuse (lights' circuit breaker) | QFN-24 4x4 mm; pin-1 dot |
| 3 | U8 | TCA9534 I/O expander (switches lights and pack path) | TSSOP-16; pin-1 dot. Not the TCA9534**A** |
| 4 | U6 | INA226 power monitor (lights' current) | VSSOP-10, 0.5 mm legs; pin-1 dot |
| 5 | U7 | TPS62933 5 V buck converter | SOT-583, the smallest chip; pin-1 mark is faint |
| 6 | U3, U4, U9 | LM74700 ideal-diode controllers (dock, pack, LEMO) | SOT-23-6; pin-1 dot |
| 7 | Q7, Q8 | 2N7002 small switches (pack off, lights off) | SOT-23, fits one way |
| 8 | D3, D4 | TPD1E10B06 ESD protection (dock signals) | 1.0 x 0.6 mm, either way round |
| 9 | C12, C16, C22 | 0.1 uF (charger supply points) | 0402 |
| 10-12 | R11, R12, R13 | 100 Ω, 5.23k, 30.1k (charger sense, thermistor bias) | 0402 |
| 13 | C25 | 4.7 uF (charger's internal regulator) | 0603 |
| 14 | C30, C31, C38, C42, C43, C47, C48 | 0.1 uF | 0603 |
| 15 | C35, C36 | 22 nF (eFuse timers) | 0603 |
| 16 | C37 | 47 nF (eFuse start-up ramp) | 0603 |
| 17 | C46 | 10 nF (buck soft start) | 0603 |
| 18 | C49, C50 | 1 uF 50 V (dock input snubber) | 0603 |
| 19-30 | R3 ... R39 | 0603 resistors, one bag per step (values on the page) | 0603 |
| 31 | LED1 | red charging LED | 0603; **polarity**: cathode on the yellow dot (check with the meter's diode setting) |
| 32 | C10, C11, C13-C15, C17-C19, C23, C24, C34 | 10 uF 25 V bulk | 0805 |
| 33 | C44, C45 | 22 uF (buck output) | 0805; keep apart from the 10 uF bag |
| 34 | C40, C41 | 10 uF 35 V (light bus) | 1206 |
| 35 | F1 | 15 A pack fuse | 1206 |
| 36 | R29 | 2 mΩ current shunt | 2512, big flat resistor |
| 37 | Q5, Q6, Q9 | power MOSFETs (ideal diodes) | SO-8FL; dot on the 4-lead side, wide tab on the big pad |
| 38 | D5 | SS34 Schottky (3.49's 5 V feed) | SMA; **band on the yellow dot** |
| 39 | D2 | **SMBJ22A** TVS (dock input) | SMB; **band on the yellow dot**; not SMBJ22**C**A |
| 40 | L1 | 2.2 uH (charger) | 7 x 6.6 mm, printed 2R2 |
| 41 | L2 | 6.8 uH (5 V buck) | same size, printed 6R8: don't swap with L1 |
| 42 | C32, C33 | 220 uF polymer (lights' output) | can on a base; **+ on the yellow dot** (cut corners of the base = +) |
| 43 | J4, J7, J8 | JST-GH 2-pin (NTC, dock signals, dock LED) | fits one way |
| 44 | J9, J11 | JST-GH 6-pin (I2C ports) | fits one way |
| 45 | J10 | JST-GH 8-pin (optional cable to the 3.49) | fits one way |

**Reflow** on the hot plate (Sn63). Chip Quik's NC191AX50 profile, board
temperatures: 25 to 100 °C in about 30 s, a slow soak from 100 to 150 °C
over about 90 s, up to 183 °C (melting) by about 150 s, peak 235 °C, and
about 90 s in all above 183 °C. The plate reads hotter than the board top
(this 4-layer, heavy-copper board lags 15-40 °C): soak with the plate at
about 150-160 °C for 60-90 s, then set about 245-250 °C and watch. Hold
until every joint has gone shiny, the QFNs have settled, and the big pads
(R29, the FETs, L1/L2) have wetted, then about 20-30 s more. Switch off;
cool on the plate or slide it level onto the heat-proof pad; don't bump it
while the solder sets. Try the plate on a spare bare board with a few dots
of leftover paste first, to learn the setting and time at which it melts.

**Inspect** under magnification: bridges on U8, U6, U7 and the QFN edges;
tombstoned (standing-up) 0402s; the orientation of every yellow-dot part.
Fix with flux and the iron (leaded wire). On U8, also look from a low angle
between the leg roots under the body, above all pins 14 and 15 (SCL, SDA):
on the first board a hidden join there shorted the I2C bus (section 6
catches it).

## 4. Bottom side: by iron (steps 46-49)

C20, C21 (10 uF 0805, the step 32 bag), C26, C27 (47 nF 0603, the step 16
bag), C28 (1 nF 0402), R10 (27.0k 0402), all under the charger. Rest the
board on its edges or a soft pad, not on the top parts. Tin one pad, slide
the part in with tweezers while reheating it, then solder the other pad.

## 5. Through-hole parts: by iron (steps 50-51)

- **J2 (dock in), J3 (pack), J5 (out to the lights)**, XT30s on large
  copper pours. Preheat the board on the hot plate at about 100-120 °C,
  use a big chisel tip at 350-380 °C and plenty of flux; the solder should
  flow through the holes. Check + is on the round pad.
- **J6** (two 1x11 female headers) or J10, per your choice. For J6, plug the
  headers onto the 3.49's pins while soldering so they line up.
- Clean the flux (IPA and a brush) around the QFNs and fine-pitch parts.

## 6. Before power: meter checks (no power, nothing connected)

Resistance to GND from each rail, on the bottom test points:
DOCK_RAW (TP5), DOCK_VIN (TP6), PACK_F (TP7), SYS (TP12), PMID (TP13),
LIGHT_BUS (TP8), LIGHT_OUT (TP9), +5V (TP10), +3V3 (TP11), REGN (TP14). None
should read near 0 Ω (large caps make the reading climb; that's fine). Also
DOCK_VIN to PACK_F, and +5V to +3V3: not shorted.

**I2C, in diode mode** (low-ohm readings on this board creep up from 0 and
can't be trusted; diode mode can). SDA is J10 pin 4 (or J6 A2), SCL J10 pin 5
(or J6 A3), GND TP1:

- SDA to SCL: not near 0 V.
- Red on TP1, black on SDA, then on SCL: about 0.5 V each (the chips'
  protection diodes).
- Black on TP1, red on SDA, then on SCL: open (or well above 1 V).

Under about 0.2 V in both directions means a short or a leak: don't power
the board until it's found. On the first board U8 shorted SDA to SCL and U6
leaked SCL to GND; the way to find which chip is to remove one at a time
(U8 first, then U6; U2 last) and measure again after each.

## 7. First power: dock input only

Nothing on J3 (pack), J5 (lights) or the 3.49.

1. Bench supply on J2: 15 V, OVP 16 V, current limit 100-200 mA. Watch the
   current as you switch on; a short shows as the supply going into
   current limit.
2. Measure: DOCK_VIN about 15 V (through U9/Q9); +5V 5.0 V (U7); LIGHT_OUT
   on J5 about 15 V (the eFuse turns on above about 11.6 V, after a ramp of
   about 170 ms); the 3.49's 5V pin on J6/J10 about 4.6-4.8 V (through D5).
   Idle current should be tens of mA. Feel for warm parts.
3. Raise the supply to 20 V (OVP 21 V), the dongle's voltage: nothing
   changes, D2 stays cool.

## 8. The 3.49

1. Power off, attach the 3.49 (J6 or J10; only ever plug or unplug the J10
   cable with the board and the 3.49 unpowered, so the I2C pins never see
   power before ground), power on from J2 again (limit
   about 500 mA now: the 3.49 and its screen draw more). The 3.49 should
   boot from the board alone.
2. On the base's USB serial: `sim off`, then `power`. The log should find
   the BQ25798 (0x6B), INA226 (0x40) and TCA9534 (0x20) and show sensible
   voltages. Try `lights off` / `lights on`: J5's voltage should switch.
   If the log shows `i2c.master: probe device timeout`, a line is held low:
   hold the 3.49's RST button (it stops the bus traffic) and measure SDA and
   SCL to TP1; both should be 3.3 V. If one is low, power off and go back to
   the I2C check in section 6.

## 9. Pack (bench supply first, then cells)

1. The bench supply on J3 as a stand-in pack, **with J2 unpowered**: about
   14.8 V, limit 500 mA. The board and the 3.49 should run from J3 alone
   (through the pack's ideal diode), and `power` should read the pack
   voltage. Don't power J2 at the same time: the charger would push current
   back into the supply, which can't absorb it.
2. Then the real pack (BMS, fuse, main switch, thermistor on J4). Check its
   polarity with the meter at the plug first. Power J2 and let it charge at
   the standard 2.0 A first, watching the STAT LED, the `power` log, the
   cell temperatures and the BMS.

## 10. Load

Lights on J5 (or a power resistor or electronic load): step up to a few amps,
then the full-white case (about 6-8 A), watching the INA226 reading, the
FETs' and R29's temperature, and the eFuse (it should not trip below 11.3 A).
