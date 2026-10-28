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
  is gone too (2026-10-27).
- PCB: one outline, about 30.2 x 100 mm. The charger section (2026-10-27) is
  a copy of [KXUSBC2](https://github.com/manuelkasper/kxusbc2)'s BQ25792 layout (cloned at
  /Volumes/Code/kxusbc2): its placement, tracks, vias and zones, rotated
  -90 degrees with U2 at (45.25, 132.4), KX-only parts dropped. Built and
  checked first in `../power_charger_kx_test/`; the scripts that did it are
  in `../power_charger_kx_test/scripts/`. Parts KX doesn't have (R14 CE,
  R15 + LED1 STAT, R37/R38 ILIM, J4 NTC) are parked right of the board, and
  VAC1/VAC2 still need a link to DOCK_VIN.
- Assembly: power, thermal-pad and fine-pitch parts on top (hot plate);
  small 0402/0603 passives may go on the bottom (hot air, after the top).
  R10-R13 and C28 are 0402 to match KX's pads. Minimums relaxed to KX's
  JLCPCB values: via 0.45 mm, annular 0.075, hole clearance and hole to
  hole 0.2.
- Test points: TP1 (GND) and TP2 (TS) 2.5 mm pads by the charger, and
  TP3-TP24, 1.5 mm pads on the bottom, parked right of the board for
  placing: GND x2, DOCK_RAW, DOCK_VIN, PACK_F, LIGHT_BUS, LIGHT_OUT, +5V,
  +3V3, SYS, PMID, REGN, EFUSE_IMON, CHG_ILIM, EFUSE_PG, EFUSE_EN,
  GATE_DOCK/PACK/LEMO, CHG_QON, DOCK_DET, DOCK_UART.
- Dock input snubber R39 (2 ohm) + C49/C50 (1 uF 50 V) on DOCK_RAW, parked
  left of the board; place it at J2.
- **4 copper layers** for the whole board (one stackup per file, and the
  charger needs its inner ground plane). The power board's net class
  widths were worked out for 2 oz outer copper on 2 layers; recheck them
  for the stackup this board is ordered with. Ground pours on In1, In2
  (charger) and the whole bottom; the XT30 grounds and the SO-8FL drain tabs
  (Q5, Q6, Q9) connect solid, not with spokes.
- Connectors: all JST-GH vertical (7.3 mm mated, under the 9.9 mm caps).
  J4 NTC and J7 LEMO data/detect (BM02B-GHS-TBT), J8 dock LED (GH2),
  J9 I2C port 1 (keypad) and J11 I2C port 2 (pole segment), same GH6
  pinout, and J10 (GH8): a cable to the 3.49 instead of the J6 headers,
  1 +5V_349, 2 GND, 3 +3V3, 4 SDA, 5 SCL, 6 INT, 7 DOCK_UART, 8 DOCK_DET.
  Each connector's job is on the silkscreen; J7's signals' ground comes
  back on J2's XT30 ground, in the same LEMO cable.
- Mounting: H1/H2 (M2.5) on the 3.49's USB-end corner holes, H3 at
  (38.5, 118.5); the XT30 end has no room. Board name and rev on the
  bottom silkscreen.
- J6 is the stacking header for the base's Waveshare ESP32-S3-Touch-LCD-3.49,
  two 1x11 2.54 mm rows matching the 3.49's own J6 (row A) and J3 (row B),
  footprint `nibbles:Waveshare_ESP32-S3-Touch-LCD-3.49_Headers`. This
  board's top faces the 3.49's flat battery side (3 switches, where its
  header pins come out), with its screen facing up (corrected 2026-10-27).
  Rows 26.491 mm apart, its PCB 29.0 x 92.5 mm; outline and all its holes
  (4x 2.72 corners, 2x 1.6, 1x 2.4, from Waveshare's STEP) on User.Drawings.
  The STEP is the footprint's 3D model, 11 mm up (8.5 mm female header plus
  the male header's plastic): put it at
  `kicad/3d/ESP32-S3-Touch-LCD-3.49.step` (git-ignored, 40 MB; from Waveshare's
  CAD download for the board).
  - A2/A3/A4 = IO1/IO2/IO3: PWR_SDA, PWR_SCL, PWR_INT. A6 = IO5: DOCK_DET.
    B6 = IO44 (U0RXD; the base's console is on USB): DOCK_UART.
  - A11 = the 3.49's 5V, which is its USB-C VBUS: fed from +5V through D5
    (SS34) so the buck can't push current into a computer on its USB port.
  - A10/B10 = its 3V3 out (+3V3 here), A9/B9 GND. Everything else is left
    open: IO47/48 are its own I2C bus (QMI8658 at 0x6B and TCA9554 at 0x20
    clash with the BQ25798 and U8), IO38-41 its SD card, IO19/20 USB, IO0
    BOOT, IO4 its battery ADC, BAT its battery.
- Net classes: the power board's (Power 5 mm, Supply 1 mm) plus Charger
  3 mm (vias 0.8/0.4) on PMID, SYS, SW1 and SW2.
