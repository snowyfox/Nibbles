# One parts order for every board

Made by `tools/hardware/combined_bom.py` from each board's BOM (the power +
charger board's JLCPCB BOM, the keypad boards' schematics) with LCSC's live
stock and prices. Rerun it after a schematic changes; change `BUILDS`,
`SPARES` or `EXTRAS` at the top of the script to order differently.

| File | What |
|---|---|
| `combined-LCSC-order.csv` | Upload to lcsc.com's BOM tool (map "LCSC Part Number" and "Quantity") |
| `combined-BOM.csv` | Every line: the part, how many each board uses x how many builds, spares, LCSC's minimum, stock, price, designators |
| `combined-optional-LCSC.csv` | GH crimp housings and contacts, for making cables (needs a fine-pitch crimper); not in the main order |

## What it's for (2026-10-30)

| Board | Builds | Needed |
|---|---|---|
| Power + charger | 2 | 1 |
| Key column (8 keys) | 4 | 2 |
| Upper keypad (3 keys) | 2 | 1 |
| Keypad hub | 2 | 1 |

Spares on top: 50% (at least 10) of the hotswap sockets and the SK6812MINI-E
LEDs, 30% (at least 2) of the ICs, 50% of connectors and passives, plus
the power board's trial values for the eFuse timing caps. LCSC's minimum
order (50-100 for most 0402/0603 passives) often adds more.

Not in it: the **Kailh Box Navy switches** themselves (not sold by LCSC),
cables, the cells and the BMS, and the PCBs.

## Side-entry JST-GH (enclosure options)

`SM02B-`, `SM06B-` and `SM08B-GHS-TB` (horizontal) are on the order as
options for the vertical `BM..B-GHS-TBT` ports. **They don't share the
vertical footprint as-is.** Comparing KiCad's two footprints: turned 180
degrees, the side-entry part's pads land within 0.1 mm of the vertical land
pattern (signal pads 1.85 vs 1.95 mm from centre, mounting pads 1.35 vs
1.40 mm), so it can be soldered on, but the **pin order is mirrored**: its
pin 1 sits on the board's pin 8 (pin 2 on pin 2 for the GH2). On such a port
the cable must be wired reversed at that end. Most of these ports carry
GND and the 3.3 V or 5 V rail on the end pins, so a straight cable there
would short or reverse power. On boards not made yet (the key column and
hub), use KiCad's `JST_GH_SM..B-GHS-TB` footprint instead and the pin order
is normal. Check JST's drawings before relying on either.
