# Building and bringing up the power + charger board

Written 2026-11-06 for the first hand-built board (JLCPCB bare board, top
stencil, LCSC parts). Build one, bring it up, then build the rest.

## Before starting

- **D2:** fit an **SMBJ22A** (from the Amazon TVS kit), not the BOM's SMBJ18A:
  the dongle gives 20 V, which an 18 V TVS would clamp.
- **Paste:** Sn63/Pb37 (NC191AX50) for the top. Out of the fridge 4 h before
  use. Keep the low-temp SnBi paste off this board's leaded joints (bismuth
  and lead make a joint that melts near 96 °C); the bottom is done with an
  iron and leaded wire instead (below).
- **Check the land patterns** against the parts in hand: L1/L2 (Chilisin
  MHCB06030 on the Sunlord footprint) and D3/D4 (TPD1E10B06, 1.0 x 0.6 mm).
- **XT30 polarity:** every XT30 has + on pad 2 (round), - on pad 1 (square).
  Check against the connector's moulded marks and your cables before soldering.
- **Choose J6 or J10** for the 3.49: J6 (stacking headers) if the board
  stacks under the 3.49 on H1/H2, J10 (GH8 cable) otherwise. Same nets.
- Kit: flux, tweezers, magnifier or microscope, meter, the programmable
  current-limited bench supply, IPA and a brush.
- ESD: wrist strap on whenever the ICs are out of their bags or the board is
  in hand. The hot plate and hot air go on a heat-proof pad (silicone or
  ceramic), not on the ESD mat: most mats are vinyl or rubber and are damaged
  by heat.
- Bench supply: set its over-voltage protection (OVP) to about 1 V above each
  test voltage, and its current limit before connecting the board.

## Top side (stencil, hot plate)

1. Hold the board flat for printing: tape it into a frame of spare boards
   of the same thickness. Align the stencil, one firm pass of the squeegee,
   lift straight up. Check every pad, especially the QFNs (U2, U5); wipe and
   reprint if anything smeared or bridged.
2. Place the top parts, smallest first, following the KiCad PCB view:
   0402/0603 passives, then SOT-23/SOT-583/VSSOP/TSSOP (U3, U4, U9, U7, U6,
   U8, Q7, Q8), the QFNs (U2, U5), SO-8FL FETs (Q5, Q6, Q9), D2/D5, F1, R29,
   L1/L2, C32/C33, LED1 (check polarity), the JST-GH connectors. Pin 1 marks:
   U2, U5, U6, U7, U8, U3/U4/U9, D3/D4. Don't fit the through-hole parts yet.
3. Reflow on the hot plate (Sn63): about 150 °C for 60-90 s, then up to
   about 215-220 °C. This 4-layer board with heavy copper heats slowly: hold
   until every joint has gone shiny, the QFNs settle, and the big pads
   (R29, the FETs, L1/L2) have wetted, then about 20-30 s more. Slide it off
   onto a heat-proof surface; don't bump it while it cools.
4. Inspect under magnification: bridges on U8 (TSSOP), U6 (VSSOP), U7
   (SOT-583), the QFN edges; tombstoned 0402s; the polarity of LED1, D2, D5.
   Fix with flux and the iron (leaded wire).

## Bottom side (iron)

5. Six parts: C20, C21 (10 uF 0805), C26, C27 (47 nF 0603), C28 (1 nF
   0402), R10 (27.0k 0402). Rest the board on its edges or a soft pad, not
   on the top parts. Tin one pad, place the part, finish the other pad.

## Through-hole parts (iron)

6. XT30s J2 (dock), J3 (pack), J5 (lights out): they sit on large pours.
   Preheat the board on the hot plate at about 100-120 °C, use a big chisel
   tip at 350-380 °C and plenty of flux; the solder should flow through the
   holes. Check + is on the round pad.
7. J6 (two 1x11 female headers) or J10, per your choice. For J6, plug the
   headers onto the 3.49's pins while soldering so they line up.
8. Clean the flux (IPA) around the QFNs and the fine-pitch parts.

## Before power: meter checks (no power, nothing connected)

9. Resistance to GND from each rail (pads or test points): DOCK_RAW,
   DOCK_VIN, PACK_F, SYS, PMID, LIGHT_BUS, LIGHT_OUT, +5V, +3V3, REGN. None
   should read near 0 Ω (large caps make the reading climb; that's fine).
   Also DOCK_VIN to PACK_F, and +5V to +3V3: not shorted.

## First power: dock input only

Nothing on J3 (pack), J5 (lights) or the 3.49.

10. Bench supply on J2: 15 V, current limit 100-200 mA. Watch the current as
    you switch on; a short shows as the supply going into current limit.
11. Measure: DOCK_VIN about 15 V (through U9/Q9); +5V 5.0 V (U7); LIGHT_OUT
    on J5 about 15 V (the eFuse turns on above about 11.6 V, after a ramp of
    about 170 ms); the 3.49's 5V pin on J6/J10 about 4.6-4.8 V (through D5).
    Idle current should be tens of mA. Feel for warm parts.
12. Raise the supply to 20 V (the dongle's voltage): nothing changes, D2
    stays cool.

## The 3.49

13. Power off, attach the 3.49 (J6 or J10), power on from J2 again (limit
    about 500 mA now, the 3.49 and its screen draw more). The 3.49 should
    boot from the board alone.
14. On the base's USB serial: `sim off`, then `power`. The log should find
    the BQ25798 (0x6B), INA226 (0x40) and TCA9534 (0x20) and show sensible
    voltages. Try `lights off` / `lights on`: J5's voltage should switch.

## Pack (bench supply first, then cells)

15. The bench supply on J3 as a stand-in pack, **with J2 unpowered**: about
    14.8 V, limit 500 mA. The board and the 3.49 should run from J3 alone
    (through the pack's ideal diode), and `power` should read the pack
    voltage. Don't power J2 at the same time: the charger would push current
    back into the supply, which can't absorb it.
16. Then the real pack (BMS, fuse, main switch, thermistor on J4). Check its
    polarity with the meter at the plug first. Power J2 and let it charge at
    the standard 2.0 A first, watching the STAT LED, the `power` log, the
    cell temperatures and the BMS.

## Load

17. Lights on J5 (or a power resistor/electronic load): step up to a few
    amps, then the full-white case (about 6-8 A), watching the INA226 reading,
    the FETs' and R29's temperature, and the eFuse (it should not trip below
    11.3 A).
