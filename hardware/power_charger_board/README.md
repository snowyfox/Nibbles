# Power board + charger (one board)

A copy of the [power board](../power_board/README.md) with the
[charger board](../charger_board/README.md) added, for laying both out as
**one board**. Made 2026-10-26 from the power board as it stood (one-sheet
schematic, placement and routing so far) and the charger board's 54 x 30 mm
placement. Open `kicad/nibbles_power_charger.kicad_pro`. The two
original projects are unchanged; this one is edited by hand in KiCad.

- Schematic: one sheet, the power board's sections with the charger below
  them as section 4. Shared nets keep their names (DOCK_VIN, PACK_F, GND,
  +3V3, +5V, PWR_SDA/SCL/INT), so they're one net across both.
- The XT30s that linked the two boards are gone: J10 and J11 on the power
  side, J21 and J22 on the charger side. DOCK_VIN and PACK_F now reach the
  charger through copper on this board. The JST-GH I2C link (J12 and J23)
  is still there; drop it too if nothing else needs it.
- PCB: the power board's outline and layout as they were, and the
  charger's 32 parts on a separate 22 x 30 mm outline beside it
  (x 176-198 on the page), with its ground plane on In1. Unrouted
  connections between the two show where they join. Merge the outlines
  into one when the layout is decided.
- **4 copper layers** for the whole board (one stackup per file, and the
  charger needs its inner ground plane). The power board's net class
  widths were worked out for 2 oz outer copper on 2 layers; recheck them
  for the stackup this board is ordered with.
- Net classes: the power board's (Power 5 mm, Supply 1 mm) plus Charger
  3 mm (vias 0.8/0.4) on PMID, SYS, SW1 and SW2.
