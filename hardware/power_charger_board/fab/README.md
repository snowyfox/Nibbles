# JLCPCB files

Made by `export.sh` from `../kicad` (close the board in KiCad first, then run
`hardware/power_charger_board/fab/export.sh`). It refills the zones and stops
if ERC, DRC or the schematic/PCB match has an error.

| File | Upload where |
|---|---|
| `nibbles_power_charger-gerbers.zip` | PCB order: "Add gerber file". 11 layers (4 copper, both masks, silk, paste, outline) + PTH and NPTH Excellon drills |
| `nibbles_power_charger-BOM-JLCPCB.csv` | PCB Assembly, BOM step (only if JLCPCB assembles) |
| `nibbles_power_charger-CPL-JLCPCB.csv` | PCB Assembly, CPL step (only if JLCPCB assembles) |
| `nibbles_power_charger-LCSC-order.csv` | lcsc.com BOM tool, to buy the parts and build by hand |
| `nibbles_power_charger-DigiKey-order.csv` | digikey.com BOM Manager (map "Manufacturer Part Number", "Quantity", "Customer Reference"): everything DigiKey can supply |
| `nibbles_power_charger-LCSC-DigiKey-gaps.csv` | lcsc.com BOM tool: the rest, when ordering from DigiKey |

## Order settings

| Setting | Choose | Why |
|---|---|---|
| Layers | 4 | |
| Dimensions | 30.2 x 100.2 mm (read from the files) | |
| Thickness | 1.6 mm | the board's stackup |
| Layer stackup | JLC04161H-7628 (the default) | no impedance control needed |
| Outer copper | 1 oz (2 oz if offered at a sensible price) | the 8-11 A light and pack paths |
| **Inner copper** | **1 oz** | not the 0.5 oz default: the In2 PACK_F, LIGHT_BUS and DOCK_VIN pours are sized for 1 oz |
| Min via hole / diameter | 0.3 mm / 0.45 mm | smallest vias on the board |
| Via covering | Tented | |
| Surface finish | ENIG | flat pads for the QFNs (U2, U5) on a hot plate; lead-free HASL works but is lumpier |
| Mark on PCB | Remove mark (or pick a spot on the bottom) | otherwise an order number lands somewhere on the silk |
| Stencil | Top (and bottom if you paste the bottom passives) | from the paste layers in the zip |

## Before ordering parts (not needed for the bare boards)

- eFuse timing caps (TPS25982 datasheet equations; 16.8 V in, 11.3 A limit):
  - C37 dVdt 47 nF: about 170 ms ramp (125 ms at the fast end). Start-up heating
    stays under about 2.5 W with up to ~2 mF on EFUSE_OUT (this board's 440 uF
    plus the controller's and strips' input caps), below the TPS259827 thermal
    shutdown curve. Trials: 22 nF (80 ms, up to ~1 mF), 100 nF (365 ms, ~4 mF).
  - C35 ITIMER 22 nF: overloads above 11.3 A are let through for about 10 ms
    (5-20 ms over tolerances) before the breaker trips; above 23.7 A it trips at
    once. Trials: 10 nF (4.7 ms), 47 nF (22 ms).
  - C36 RETRY_DLY 22 nF: after a trip, retries every 1 s, indefinitely (NRETRY
    is grounded). Trials: 47 nF (2.2 s), 100 nF (4.7 s).
- `nibbles_power_charger-LCSC-order.csv` is the LCSC order for building by hand
  (LCSC's BOM tool: upload, map "LCSC Part Number" and "Quantity"). Quantities
  are per board x `BOARDS` (default 2: `BOARDS=5 ./export.sh`), plus 10 of each
  timing-cap trial value, and both of J6's headers.
- R27 (eFuse IMON) is 1k: the datasheet's example is 887 ohm for 3.3 V at
  15 A, so 1k gives about 2.8 V at this board's 11.3 A limit (on TP15).
- Every BOM line has an LCSC number (in the schematic's `LCSC` fields), one
  line per part number, as JLCPCB's BOM check wants.
- The part numbers are picked to be in stock at LCSC, for buying the parts
  and building the board by hand. Several are JLCPCB "Extended" parts, so for a
  JLCPCB-assembled order, swapping to their Basic equivalents saves setup fees
  (C131394->C307331, C106205->C1523, C1589->C57112, C89632->C13585, C98220->C25804).
- L1 and L2 are Chilisin MHCB06030 (C285810, C285805): 7 x 6.6 x 3.0 mm, the
  same size as the Sunlord MWSA0603S footprint they sit on. Check their land
  pattern (datasheet page 7) against the footprint before ordering.
- J6 is one BOM line but two 1x11 female headers (C2932674) per board.
- Rotations in the CPL are KiCad's; check each part in JLCPCB's placement
  preview, since some of their library parts are rotated differently.
- J6 (the 3.49's stacking headers) and the XT30s are through-hole: fit them
  by hand, or leave them out of an assembly order.
- DigiKey parts come from `digikey_map.csv` (one row per LCSC number; an empty
  part number means buy it from LCSC). Its stock column is a snapshot from
  2026-10-28. Substitutions to know about: C32/C33 become 150 uF Panasonic
  hybrids (no 220 uF 25 V in a 6.3 mm can was in stock), L1/L2 Eaton HCM0703,
  R29 a 1 W Vishay WSL2512 (0.26 W at the limit), J6 Sullins PPTC111LFBN-RC.
