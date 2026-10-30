# JLCPCB files for the keypad boards

Made by `export.sh` from the KiCad projects next to this folder. Close the board
in KiCad first, then run `hardware/keypad/fab/export.sh key_upper` (or
`key_column`, `key_hub` once they're routed). The script refills the zones,
and it stops if ERC, DRC or the schematic/PCB match has an error. Each board
gets its own folder here.

| File | Upload where |
|---|---|
| `<board>-gerbers.zip` | PCB order: "Add gerber file". 9 layers (2 copper, both masks, silk, paste, outline) + PTH and NPTH Excellon drills |
| `<board>-BOM-JLCPCB.csv` | PCB Assembly, BOM step (only if JLCPCB assembles) |
| `<board>-CPL-JLCPCB.csv` | PCB Assembly, CPL step (only if JLCPCB assembles) |
| `<board>-LCSC-order.csv` | lcsc.com BOM tool (map "LCSC Part Number" and "Quantity"), to buy the parts and build by hand; quantities are per board x `BOARDS` (default 2: `BOARDS=5 ./export.sh key_upper`) |

Holes (M2 mounting holes) and solder jumpers have no LCSC number, so they're
left out of the BOM and placement files.

## Order settings (key_upper)

| Setting | Choose | Why |
|---|---|---|
| Layers | 2 | |
| Dimensions | 18 x 76.2 mm (read from the files) | |
| Thickness | 1.6 mm | the hotswap sockets and the MX pegs expect a 1.6 mm board |
| Copper | 1 oz | |
| Min via hole / diameter | 0.3 mm / 0.6 mm | |
| Via covering | Tented | |
| Surface finish | HASL (lead-free) is fine; ENIG if it's cheap | TSSOP-16 at 0.65 mm pitch is the finest part |
| Mark on PCB | Remove mark (or pick a spot on the front) | |
| Stencil | **Bottom only** | every SMD part is on the back; the front paste layer is empty |

## Notes

- **Every part is on the back**, including the SK6812MINI-E LEDs, which are
  reverse-mount and shine through their cutouts to the front.
- **SW1-SW3** in the files are the Kailh CPG151101S11 hotswap sockets, which
  solder to the back. The Box Navy switches press into them from the front;
  they aren't on the BOM. The CPL puts the sockets on the bottom layer.
- Rotations in the CPL are KiCad's. If JLCPCB assembles, check each part in
  their placement preview, since some of their library parts are rotated
  differently. Bottom-side assembly costs extra, and the hotswap sockets and
  reverse-mount LEDs are easier to fit by hand.
- **Address jumpers:** JP1-JP3 (A0-A2) are bridged 1-2 as made, which gives
  address 0x38. Give every key board on the bus its own address: cut 1-2 and
  bridge 2-3 to set a bit to 1 (see `../README.md`).
