#!/usr/bin/env python3
"""Make the power + charger board's step-by-step placement page.

    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
        tools/hardware/assembly_guide.py

Reads hardware/power_charger_board/kicad/nibbles_power_charger.kicad_pcb (with
KiCad's own Python, for pcbnew) and writes
hardware/power_charger_board/assembly/placement.html: one self-contained page
with every assembly step (which bag, what the part is, how to recognise and
orient it) and a map of the board with that step's parts highlighted. Ticks
are kept in the browser. The steps and their text are in STEPS below; the
script stops if a part on the board is in no step or in two.
"""
import html
import json
import os
import sys

import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
PCB = os.path.join(ROOT, "hardware", "power_charger_board", "kicad", "nibbles_power_charger.kicad_pcb")
OUT = os.path.join(ROOT, "hardware", "power_charger_board", "assembly", "placement.html")

# Pad 1 meaning, per kind of part: shown as a dot on the map and in the step.
PIN1 = {
    "ic": "pin 1: the dot or notch on the chip",
    "diode": "pad 1 is the cathode: the band on the part",
    "led": "pad 1 is the cathode",
    "cap+": "pad 1 is +: the cut corners on the base; the dark mark on the top is -",
    "xt30": "+ is pad 2 (the round pad), - is pad 1 (square)",
}

# Assembly order. side: top (stencil + hot plate), bottom (iron), tht (iron).
# bag: what to look for on the bag label. Text is for someone holding the
# part in tweezers.
STEPS = [
    # --- top: chips first, while the paste is fresh and the board is clear
    dict(side="top", refs="U2", bag="C2876593 / BQ25798RQMR", pkg="QFN-29, 4 x 4 mm, no legs",
         what="Battery charger chip (charges the internal pack from the dock or USB-C).",
         look="Small black square with pads only underneath and a large centre pad. Laser-marked; pin-1 dot in a corner.",
         pin1="ic", tip="Line it up by eye on the paste, then let it be: it centres itself when the solder melts. Don't press it into the paste."),
    dict(side="top", refs="U5", bag="C2155765 / TPS259827ONRGET", pkg="QFN-24, 4 x 4 mm, two pads underneath",
         what="eFuse: the electronic circuit breaker for the lights' output.",
         look="Black square, pads underneath (two separate large pads). Pin-1 dot in a corner.",
         pin1="ic", tip="Same as U2: place lightly, the solder pulls it straight."),
    dict(side="top", refs="U8", bag="C783615 / TCA9534PWR", pkg="TSSOP-16 (5 x 4.4 mm, 8 legs a side)",
         what="I/O expander: lets the 3.49 switch the lights and the pack path over I2C.",
         look="Thin black chip with 8 legs on each long side. Pin-1 dot or a dimple at one end.",
         pin1="ic", tip="Not the TCA9534**A**PWR (C206010): that one is for the keypad boards and has a different I2C address."),
    dict(side="top", refs="U6", bag="C49851 / INA226AIDGSR", pkg="VSSOP-10 (3 x 3 mm, 5 legs a side)",
         what="Power monitor: measures the lights' current through R29 and the voltage.",
         look="Tiny black chip, 5 legs per side, 0.5 mm apart. Pin-1 dot.",
         pin1="ic", tip="Legs are close together: check for bridges after reflow."),
    dict(side="top", refs="U7", bag="C3200405 / TPS62933DRLR", pkg="SOT-583 (2.1 x 1.6 mm, 8 legs)",
         what="5 V buck converter: makes 5 V for the 3.49 from the pack or dock.",
         look="Very small black part with 4 short legs on each long side. Pin-1 mark is faint: use the magnifier.",
         pin1="ic", tip="The smallest IC here. Easy to place 180 degrees wrong: check pin 1 twice."),
    dict(side="top", refs="U3,U4,U9", bag="C2941042 / LM74700QDBVRQ1", pkg="SOT-23-6 (6 legs, 3 a side)",
         what="Ideal-diode controllers: U9 for the LEMO input, U3 for the dock, U4 for the pack. Each drives one of the big FETs, so power flows from the dock or the pack to the lights and never back.",
         look="Small black part, 3 legs on each side.",
         pin1="ic", tip="Pin 1 is next to the dot or the bevelled edge; the map's dot is pin 1."),
    dict(side="top", refs="Q7,Q8", bag="C8545 / 2N7002", pkg="SOT-23 (3 legs)",
         what="Small switching transistors driven by the expander (U8): Q7 turns the pack's path off, Q8 turns the lights' eFuse off.",
         look="Small black part with 2 legs on one side, 1 on the other. Only fits one way round.",
         pin1=None, tip=None),
    dict(side="top", refs="D3,D4", bag="C48260 / TPD1E10B06DPYR", pkg="1.0 x 0.6 mm, 2 pads",
         what="ESD protection for the dock's data and detect wires.",
         look="Tiny, like an 0402 resistor but black and slightly thicker.",
         pin1=None, tip="Works either way round (bidirectional). Easy to lose: open the bag over the tray."),
    # --- 0402 (the smallest passives)
    dict(side="top", refs="C12,C16,C22", bag="C131394 / CC0402KRX7R9BB104 (0.1 uF)", pkg="0402 capacitor",
         what="Small capacitors on the charger's three supply points (dock input, PMID, SYS).",
         look="Tiny tan/brown block, no marking.", pin1=None, tip="Capacitors aren't marked: keep each bag closed until its step."),
    dict(side="top", refs="R11", bag="C25076 / 0402WGF1000TCE (100 ohm)", pkg="0402 resistor",
         what="Filters the charger's battery-voltage sense line.", look="Tiny black-topped block (often unmarked at 0402).", pin1=None, tip=None),
    dict(side="top", refs="R12", bag="C2933105 / FRC0402F5231TS (5.23k 1%)", pkg="0402 resistor",
         what="Thermistor bias (top half of the TS divider, with R13 and the pack NTC on J4).", look="Tiny black-topped block.", pin1=None, tip=None),
    dict(side="top", refs="R13", bag="C2998131 / FRC0402F3012TS (30.1k 1%)", pkg="0402 resistor",
         what="Thermistor bias (bottom half of the TS divider).", look="Tiny black-topped block.", pin1=None, tip=None),
    # --- 0603 capacitors
    dict(side="top", refs="C25", bag="C19666 / CL10A475KO8NNNC (4.7 uF 10 V)", pkg="0603 capacitor",
         what="Charger chip's internal regulator capacitor.", look="Small tan block, no marking.", pin1=None, tip=None),
    dict(side="top", refs="C30,C31,C38,C42,C43,C47,C48", bag="C14663 / CC0603KRX7R9BB104 (0.1 uF)", pkg="0603 capacitor",
         what="C30, C31, C48: the ideal-diode controllers' charge-pump capacitors. C38, C47: decoupling on 3.3 V. C42: on the light bus. C43: the 5 V buck's bootstrap capacitor.", look="Small tan block, no marking.", pin1=None,
         tip="Shared bag with the keypad boards (200 ordered)."),
    dict(side="top", refs="C35,C36", bag="C21122 / CL10B223KB8NNNC (22 nF)", pkg="0603 capacitor",
         what="eFuse timing: C35 the overload timer, C36 the retry delay.", look="Small tan block, no marking.", pin1=None, tip=None),
    dict(side="top", refs="C37", bag="C1622 / CL10B473KB8NNNC (47 nF)", pkg="0603 capacitor",
         what="eFuse start-up ramp (about 170 ms).", look="Small tan block, no marking.", pin1=None,
         tip="Same bag as C26/C27 on the bottom."),
    dict(side="top", refs="C46", bag="C1589 / CL10B103KB8NNNC (10 nF)", pkg="0603 capacitor",
         what="5 V buck's soft-start capacitor.", look="Small tan block, no marking.", pin1=None, tip=None),
    dict(side="top", refs="C49,C50", bag="C77386 / GRM188R61H105KAALD (1 uF 50 V)", pkg="0603 capacitor",
         what="Dock input snubber (with R39), against voltage spikes when the dock plugs in.", look="Small tan block, no marking.", pin1=None,
         tip="This is the substitute LCSC sent instead of C15849: same value."),
    # --- 0603 resistors
    dict(side="top", refs="R3,R4,R27", bag="C21190 / 0603WAF1001T5E (1k)", pkg="0603 resistor",
         what="R3/R4: series resistors on the dock's data and detect wires. R27: the eFuse's current-monitor resistor.",
         look="Small black-topped block.", pin1=None, tip=None),
    dict(side="top", refs="R5,R22,R25", bag="C25803 / 0603WAF1003T5E (100k)", pkg="0603 resistor",
         what="R5: holds the dock-detect line low with no dock. R22, R25: keep the pack and lights switched on until the base says otherwise.", look="Small black-topped block.", pin1=None, tip="Shared bag with the keypad hub."),
    dict(side="top", refs="R14,R16,R24,R28,R31,R37", bag="C98220 / RC0603FR-0710KL (10k)", pkg="0603 resistor",
         what="R14: enables charging. R16: interrupt line pull-up. R24: eFuse turn-on divider (with R23). R28: eFuse power-good pull-up. R31: 5 V buck divider (with R30). R37: charger input-current setting (with R38).", pin1=None, tip=None),
    dict(side="top", refs="R15", bag="C4190 / 0603WAF2201T5E (2.2k)", pkg="0603 resistor",
         what="Current limit for the charging LED.", pin1=None, tip=None),
    dict(side="top", refs="R20,R21,R35", bag="C22935 / 0603WAF1004T5E (1M)", pkg="0603 resistor",
         what="Enable pull-ups for the three ideal-diode controllers (R35 LEMO, R20 dock, R21 pack).", pin1=None, tip=None),
    dict(side="top", refs="R23", bag="C23121 / 0603WAF8662T5E (86.6k 1%)", pkg="0603 resistor",
         what="eFuse turn-on divider (on at about 11.6 V, with R24).", pin1=None, tip=None),
    dict(side="top", refs="R26", bag="C22796 / 0603WAF1300T5E (130 ohm 1%)", pkg="0603 resistor",
         what="Sets the eFuse's current limit (about 11.3 A).", pin1=None, tip=None),
    dict(side="top", refs="R30", bag="C2933235 / FRC0603F5232TS (52.3k 1%)", pkg="0603 resistor",
         what="5 V buck's output voltage divider.", pin1=None, tip=None),
    dict(side="top", refs="R32,R33", bag="C23162 / 0603WAF4701T5E (4.7k)", pkg="0603 resistor",
         what="I2C pull-ups (SDA, SCL).", pin1=None, tip=None),
    dict(side="top", refs="R36", bag="C23234 / 0603WAF7501T5E (7.5k)", pkg="0603 resistor",
         what="Current limit for the dock's LED.", pin1=None, tip=None),
    dict(side="top", refs="R38", bag="C22857 / 0603WAF1072T5E (10.7k 1%)", pkg="0603 resistor",
         what="Charger input current-limit setting (with R37).", pin1=None, tip=None),
    dict(side="top", refs="R39", bag="C22977 / 0603WAF200KT5E (2 ohm)", pkg="0603 resistor",
         what="Dock input snubber (with C49/C50).", pin1=None, tip=None),
    dict(side="top", refs="LED1", bag="C2286 / KT-0603R (red LED)", pkg="0603 LED",
         what="Charging status LED.", look="Tiny clear block; a coloured mark on one end usually shows the cathode, but makers differ.",
         pin1="led", tip="Polarity matters. Check it before placing: on the meter's diode setting it glows faintly with the red lead on the anode, so the black lead is on the cathode."),
    # --- 0805 and 1206
    dict(side="top", refs="C10,C11,C13,C14,C15,C17,C18,C19,C23,C24,C34", bag="C15850 / CL21A106KAYNNNE (10 uF 25 V)", pkg="0805 capacitor",
         what="Bulk capacitors: C10/C11 on the dock input, C13-C15 on PMID, C17-C19 on SYS, C23/C24 on the pack, C34 on the light bus.", look="Tan block, 2 x 1.25 mm, no marking.", pin1=None,
         tip="11 on the top; C20 and C21 from the same bag go on the bottom later. Shared bag with the keypad boards."),
    dict(side="top", refs="C44,C45", bag="C45783 / CL21A226MAQNNNE (22 uF 10 V)", pkg="0805 capacitor",
         what="5 V buck's output capacitors.", look="Tan block like the 10 uF ones: keep the bags apart.", pin1=None,
         tip="Shared bag with the keypad hub."),
    dict(side="top", refs="C40,C41", bag="C89632 / CL31B106KBHNNNE (10 uF 35 V)", pkg="1206 capacitor",
         what="Capacitors on the light bus, which also feeds the 5 V buck.", look="Larger tan block, 3.2 x 1.6 mm.", pin1=None, tip=None),
    dict(side="top", refs="F1", bag="C2443310 / CC12H15A-TR (15 A fuse)", pkg="1206 fuse",
         what="The pack fuse.", look="1206 block, white or black body, often marked with a letter code.", pin1=None, tip=None),
    # --- power parts
    dict(side="top", refs="R29", bag="C154685 / LR2512-23R002F4 (2 mOhm)", pkg="2512 resistor (6.3 x 3.2 mm)",
         what="Current-sense shunt for the lights (read by U6).", look="Big flat resistor with wide metal ends, printed R002.", pin1=None, tip=None),
    dict(side="top", refs="Q5,Q6,Q9", bag="C179623 / NTMFS4C022NT1G", pkg="SO-8FL (5 x 6 mm, flat leads)",
         what="Power MOSFETs used as ideal diodes: Q9 from the LEMO to the dock rail, Q5 from the dock to the light bus, Q6 from the pack to the light bus.",
         look="Black rectangle, 4 small leads on one side, a wide metal tab on the other.",
         pin1="ic", tip="Pin 1 dot on the side with 4 separate leads; the wide tab (drain) goes on the big pad."),
    dict(side="top", refs="D5", bag="C8678 / SS34", pkg="SMA diode (4.3 x 2.6 mm)",
         what="Schottky diode feeding the 3.49's 5 V pin (stops back-feeding a computer on its USB).",
         look="Black block with a grey band at one end.", pin1="diode", tip="Band (cathode) on pad 1: the map's dot."),
    dict(side="top", refs="D2", bag="SMBJ22A from the Amazon TVS kit (not the LCSC SMBJ18A, C10212)", pkg="SMB diode (4.6 x 3.6 mm)",
         what="TVS: clamps voltage spikes on the dock input. 22 V because the dongle gives 20 V.",
         look="Black block with a band at one end and a printed code.",
         pin1="diode", tip="Band (cathode) on pad 1. Make sure it's SMBJ22A (unidirectional), not SMBJ22CA."),
    dict(side="top", refs="L1", bag="C285810 / MHCB06030-2R2M-C1 (2.2 uH)", pkg="7 x 6.6 x 3 mm inductor",
         what="The charger's inductor.", look="Grey/black block, printed 2R2.", pin1=None,
         tip="L1 and L2 look the same: 2R2 here, 6R8 for L2."),
    dict(side="top", refs="L2", bag="C285805 / MHCB06030-6R8M-C1 (6.8 uH)", pkg="7 x 6.6 x 3 mm inductor",
         what="The 5 V buck's inductor.", look="Grey/black block, printed 6R8.", pin1=None, tip=None),
    dict(side="top", refs="C32,C33", bag="C2981459 / KNM2220UF25V149EC0051 (220 uF 25 V polymer)", pkg="6.3 x 9.9 mm can on a plastic base",
         what="Bulk capacitors on the lights' output.", look="Small aluminium can on a square plastic base.",
         pin1="cap+", tip="Polarity matters: + to the map's dot. The base's cut corners are on the + side; the dark mark on the top of the can is -."),
    # --- SMD connectors (tallest, last)
    dict(side="top", refs="J4,J7,J8", bag="C161690 / BM02B-GHS-TBT (JST-GH 2-pin, vertical)", pkg="JST-GH 2-pin vertical",
         what="J4: the pack thermistor. J7: dock data and detect. J8: the dock's LED.",
         look="Small white connector with 2 pins and 2 side tabs.", pin1=None, tip="Only fits the pads one way: the 2 tabs go on the 2 big pads."),
    dict(side="top", refs="J9,J11", bag="C189892 / BM06B-GHS-TBT (JST-GH 6-pin, vertical)", pkg="JST-GH 6-pin vertical",
         what="I2C ports: J9 to the keypad, J11 to the pole segment.", look="White connector, 6 pins.", pin1=None, tip=None),
    dict(side="top", refs="J10", bag="C133062 / BM08B-GHS-TBT (JST-GH 8-pin, vertical)", pkg="JST-GH 8-pin vertical",
         what="Optional cable to the 3.49 instead of the J6 headers.", look="White connector, 8 pins.", pin1=None,
         tip="Fit it only if the 3.49 will be cabled instead of stacked (it does no harm either way). Shared bag with the keypad boards."),
    # --- bottom, by iron, after the top is reflowed
    dict(side="bottom", refs="C20,C21", bag="C15850 (10 uF 25 V), same bag as step 32", pkg="0805 capacitor",
         what="More capacitors on the charger's SYS rail, under U2.", look="Tan block, no marking.", pin1=None,
         tip="Rest the board on its edges or a soft pad so the top parts don't take the weight."),
    dict(side="bottom", refs="C26,C27", bag="C1622 (47 nF), same bag as step 16 (C37)", pkg="0603 capacitor",
         what="The charger's two bootstrap capacitors (C26 BTST1, C27 BTST2).", look="Small tan block, no marking.", pin1=None, tip=None),
    dict(side="bottom", refs="C28", bag="C106205 / CC0402KRX7R9BB102 (1 nF)", pkg="0402 capacitor",
         what="The charger's ship-FET driver (SDRV) capacitor.", look="Tiny tan block.", pin1=None, tip=None),
    dict(side="bottom", refs="R10", bag="C25771 / 0402WGF2702TCE (27.0k 1%)", pkg="0402 resistor",
         what="The charger's PROG resistor: sets cell count and switching frequency at power-up.", look="Tiny black-topped block.", pin1=None, tip=None),
    # --- through-hole, by iron
    dict(side="tht", refs="J2,J3,J5", bag="C431092 / XT30PW-M30.G.Y", pkg="XT30 right-angle PCB socket",
         what="J2: dock power in (from the LEMO). J3: the internal pack. J5: out to the lights' converter.",
         look="Yellow XT30 with two pins and two plastic pegs.", pin1="xt30",
         tip="Preheat the board (hot plate about 110 C), big chisel tip at 350-380 C, plenty of flux. Check + (pad 2, round) against the connector's moulded marks."),
    dict(side="tht", refs="J6", bag="C2932674 / KH-2.54FH-1X11P-H8.5 (two per board)", pkg="2 x 1x11 female headers, 2.54 mm",
         what="Stacking headers for the 3.49.", look="Black 11-way female header strips.", pin1=None,
         tip="Plug both strips onto the 3.49's pins while soldering, so they line up. Skip if using J10's cable instead."),
]

SIDE_TITLE = {"top": "Top side: stencil, place, hot plate",
              "bottom": "Bottom side: by iron, after the top",
              "tht": "Through-hole parts: by iron"}


def mm(v):
    return round(pcbnew.ToMM(v), 3)


def main():
    board = pcbnew.LoadBoard(PCB)
    outline = pcbnew.SHAPE_POLY_SET()
    board.GetBoardPolygonOutlines(outline, True)
    ol = outline.Outline(0)
    pts = [(mm(ol.CPoint(i).x), mm(ol.CPoint(i).y)) for i in range(ol.PointCount())]
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    box = (min(xs), min(ys), max(xs), max(ys))

    parts = {}
    for fp in board.GetFootprints():
        ref = fp.GetReference()
        pads = []
        for p in fp.Pads():
            bb = p.GetBoundingBox()
            pads.append(dict(n=p.GetNumber(), x=mm(bb.GetX()), y=mm(bb.GetY()), w=mm(bb.GetWidth()), h=mm(bb.GetHeight()),
                             round=p.GetShape() in (pcbnew.PAD_SHAPE_CIRCLE, pcbnew.PAD_SHAPE_OVAL),
                             tht=p.GetAttribute() in (pcbnew.PAD_ATTRIB_PTH, pcbnew.PAD_ATTRIB_NPTH)))
        bb = fp.GetBoundingBox(False, False)
        parts[ref] = dict(ref=ref, value=fp.GetValue(), bottom=fp.IsFlipped(), pads=pads,
                          bx=mm(bb.GetX()), by=mm(bb.GetY()), bw=mm(bb.GetWidth()), bh=mm(bb.GetHeight()))

    seen = {}
    for i, s in enumerate(STEPS):
        s["refs"] = s["refs"].split(",")
        for r in s["refs"]:
            if r not in parts:
                sys.exit(f"step {i + 1}: {r} is not on the board")
            if r in seen:
                sys.exit(f"{r} is in steps {seen[r] + 1} and {i + 1}")
            seen[r] = i
            if parts[r]["bottom"] != (s["side"] == "bottom"):
                sys.exit(f"{r}: step side {s['side']} but the board has it on the {'bottom' if parts[r]['bottom'] else 'top'}")
        s["pin1text"] = PIN1.get(s["pin1"]) if s["pin1"] else None
    skip = ("H", "TP", "LOGO")
    missing = [r for r in parts if r not in seen and not r.startswith(skip)]
    if missing:
        sys.exit("not in any step: " + ", ".join(sorted(missing)))

    data = dict(box=box, outline=pts, parts=parts, steps=STEPS, sides=SIDE_TITLE)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    page = TEMPLATE.replace("/*DATA*/null", json.dumps(data, separators=(",", ":")))
    open(OUT, "w").write(page)
    print(f"{len(STEPS)} steps, {len(seen)} parts -> {os.path.relpath(OUT, ROOT)}")


TEMPLATE = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Power Board Placement</title>
<style>
:root {
  --bg: #f6f5f1; --panel: #ffffff; --ink: #1d1d1b; --muted: #6b6a64; --line: #dcdad2;
  --board: #2f5d3a; --edge: #1f3d27; --pad: #c9b98a; --pad-dim: #7d8f74; --done: #8fa8c9;
  --hi: #ff4f2e; --hi-soft: rgba(255, 79, 46, .18); --pin1: #ffe14d; --label: #ffffff;
  --accent: #c2410c;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    --bg: #161614; --panel: #201f1c; --ink: #ecebe6; --muted: #9b998f; --line: #3a3934;
    --board: #24452c; --edge: #12261a; --pad: #b8a874; --pad-dim: #56664f; --done: #5c7aa3;
    --hi: #ff6a4a; --hi-soft: rgba(255, 106, 74, .22); --accent: #fb923c;
  }
}
:root[data-theme="dark"] {
  --bg: #161614; --panel: #201f1c; --ink: #ecebe6; --muted: #9b998f; --line: #3a3934;
  --board: #24452c; --edge: #12261a; --pad: #b8a874; --pad-dim: #56664f; --done: #5c7aa3;
  --hi: #ff6a4a; --hi-soft: rgba(255, 106, 74, .22); --accent: #fb923c;
}
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--ink);
  font: 15px/1.45 -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
header { padding: 14px 16px 6px; }
h1 { font-size: 19px; margin: 0 0 2px; }
.sub { color: var(--muted); font-size: 13px; }
/* the page itself never scrolls: the map card stays put and the step list scrolls on its own */
html, body { height: 100%; }
body { display: flex; flex-direction: column; height: 100vh; height: 100dvh; overflow: hidden; }
header { flex: none; }
.wrap { flex: 1; min-height: 0; display: grid; grid-template-columns: minmax(300px, 420px) 1fr;
  grid-template-rows: minmax(0, 1fr); gap: 14px; padding: 8px 16px 12px; }
.list { min-height: 0; overflow-y: auto; overscroll-behavior: contain; padding-right: 4px; }
.side { font-weight: 600; font-size: 13px; text-transform: uppercase; letter-spacing: .04em;
  color: var(--muted); margin: 14px 0 6px; }
.step { display: flex; gap: 10px; align-items: flex-start; padding: 8px 10px; margin-bottom: 4px;
  border: 1px solid var(--line); border-radius: 8px; background: var(--panel); cursor: pointer; }
.step.cur { border-color: var(--hi); box-shadow: 0 0 0 2px var(--hi-soft); }
.step.done { opacity: .55; }
.step input { margin-top: 3px; width: 18px; height: 18px; accent-color: var(--accent); flex: none; }
.step .n { color: var(--muted); font-variant-numeric: tabular-nums; min-width: 1.8em; }
.step .t { font-weight: 600; }
.step .r { color: var(--muted); font-size: 13px; }
.view { min-height: 0; min-width: 0; overflow-y: auto; }
.card { background: var(--panel); border: 1px solid var(--line); border-radius: 10px; padding: 12px 14px; }
.card h2 { font-size: 17px; margin: 0 0 4px; }
.card .refs { font-family: ui-monospace, Menlo, monospace; font-size: 14px; color: var(--accent); }
.card dl { display: grid; grid-template-columns: max-content 1fr; gap: 3px 12px; margin: 8px 0 0; font-size: 14px; }
.card dt { color: var(--muted); }
.card dd { margin: 0; }
svg { width: 100%; height: auto; display: block; margin-top: 10px; background: var(--panel); }
.nav { display: flex; gap: 8px; margin-top: 10px; flex-wrap: wrap; align-items: center; }
button { font: inherit; padding: 6px 12px; border-radius: 7px; border: 1px solid var(--line);
  background: var(--panel); color: var(--ink); cursor: pointer; }
button.primary { background: var(--accent); border-color: var(--accent); color: #fff; }
.legend { color: var(--muted); font-size: 13px; margin-top: 6px; }
.prog { color: var(--muted); font-size: 13px; margin-left: auto; }
@media (max-width: 760px) {
  header { padding-bottom: 2px; }
  header .sub { display: none; }
  .wrap { grid-template-columns: minmax(0, 1fr); grid-template-rows: auto minmax(0, 1fr); gap: 10px; }
  .view { order: -1; max-height: 62vh; max-height: 62dvh; }
}
</style>
</head>
<body>
<header>
  <h1>Power + charger board: placement</h1>
  <div class="sub">One step per bag. Click a step (or use ← →); its parts light up on the map. Tick it when placed.
  Map: XT30 end (J2, J5) on the left; top side as you look down on it, bottom side as you see it turned over top-to-bottom.</div>
</header>
<div class="wrap">
  <div class="list" id="list"></div>
  <div class="view">
    <div class="card" id="card"></div>
  </div>
</div>
<script>
const D = /*DATA*/null;
const KEY = "nibbles-power-placement";
let done = {};
try { done = JSON.parse(localStorage.getItem(KEY) || "{}"); } catch (e) {}
function save() { try { localStorage.setItem(KEY, JSON.stringify(done)); } catch (e) {} }
let cur = 0;
const [X0, Y0, X1, Y1] = D.box;
const L = Y1 - Y0, W = X1 - X0, M = 3;
// board long axis (KiCad y) runs left to right; top view keeps handedness, bottom view is turned over
function tx(x, y, bottom) { return [y - Y0 + M, bottom ? x - X0 + M : X1 - x + M]; }
function rect(p, bottom, cls, extra) {
  const a = tx(p.x, p.y, bottom), b = tx(p.x + p.w, p.y + p.h, bottom);
  const x = Math.min(a[0], b[0]), y = Math.min(a[1], b[1]), w = Math.abs(a[0] - b[0]), h = Math.abs(a[1] - b[1]);
  const r = p.round ? Math.min(w, h) / 2 : Math.min(w, h) * .15;
  return `<rect x="${x.toFixed(3)}" y="${y.toFixed(3)}" width="${w.toFixed(3)}" height="${h.toFixed(3)}" rx="${r.toFixed(3)}" ${cls} ${extra || ""}/>`;
}
function pin1Pad(part, kind) {
  const want = kind === "xt30" ? "2" : "1";
  return part.pads.find(p => p.n === want);
}
function draw(step) {
  const side = step.side === "bottom";
  const hi = new Set(step.refs);
  const doneRefs = new Set();
  D.steps.forEach((s, i) => { if (done[i]) s.refs.forEach(r => doneRefs.add(r)); });
  let s = `<svg viewBox="0 0 ${L + 2 * M} ${W + 2 * M}" xmlns="http://www.w3.org/2000/svg">`;
  const o = D.outline.map(([x, y]) => tx(x, y, side).map(v => v.toFixed(3)).join(",")).join(" ");
  s += `<polygon points="${o}" fill="var(--board)" stroke="var(--edge)" stroke-width=".4"/>`;
  for (const ref in D.parts) {
    const p = D.parts[ref];
    const onSide = p.pads.some(q => q.tht) || p.bottom === side;
    if (!onSide || hi.has(ref)) continue;
    const fill = doneRefs.has(ref) ? "var(--done)" : "var(--pad-dim)";
    for (const q of p.pads) s += rect(q, side, `fill="${fill}"`);
  }
  for (const ref of step.refs) {
    const p = D.parts[ref];
    s += rect({x: p.bx, y: p.by, w: p.bw, h: p.bh}, side, `fill="var(--hi-soft)" stroke="var(--hi)" stroke-width=".35"`);
    for (const q of p.pads) s += rect(q, side, `fill="var(--hi)"`);
    if (step.pin1) {
      const q = pin1Pad(p, step.pin1);
      if (q) { const c = tx(q.x + q.w / 2, q.y + q.h / 2, side);
        s += `<circle cx="${c[0].toFixed(3)}" cy="${c[1].toFixed(3)}" r=".55" fill="var(--pin1)" stroke="#000" stroke-width=".12"/>`; }
    }
    const c = tx(p.bx + p.bw / 2, p.by + p.bh / 2, side);
    const ly = c[1] < W / 2 + M ? c[1] + Math.abs(p.bw) / 2 + 2.6 : c[1] - Math.abs(p.bw) / 2 - 1.2;
    s += `<text x="${c[0].toFixed(2)}" y="${Math.max(2.2, Math.min(W + 2 * M - .6, ly)).toFixed(2)}" font-size="2.1" font-weight="700" text-anchor="middle" fill="var(--label)" stroke="var(--edge)" stroke-width=".5" paint-order="stroke">${ref}</text>`;
  }
  for (const ref of ["J2", "J5", "J3"]) {
    if (hi.has(ref)) continue;
    const p = D.parts[ref], c = tx(p.bx + p.bw / 2, p.by + p.bh / 2, side);
    s += `<text x="${c[0].toFixed(2)}" y="${c[1].toFixed(2)}" font-size="1.8" text-anchor="middle" fill="var(--label)" opacity=".75">${ref}</text>`;
  }
  return s + "</svg>";
}
function esc(t) { return String(t).replace(/[&<>]/g, c => ({"&": "&amp;", "<": "&lt;", ">": "&gt;"}[c])); }
function md(t) { return esc(t).replace(/\*\*(.+?)\*\*/g, "<b>$1</b>"); }
function renderCard() {
  const s = D.steps[cur];
  const vals = [...new Set(s.refs.map(r => D.parts[r].value))].join(" / ");
  const rows = [["Bag", s.bag], ["Part", s.pkg], ["Value", vals], ["What it does", s.what]];
  if (s.look) rows.push(["Looks like", s.look]);
  if (s.pin1text) rows.push(["Orientation", s.pin1text + " (yellow dot on the map)"]);
  if (s.tip) rows.push(["Tip", s.tip]);
  const n = D.steps.filter((_, i) => done[i]).length;
  document.getElementById("card").innerHTML =
    `<div class="sub">Step ${cur + 1} of ${D.steps.length} · ${esc(D.sides[s.side])}</div>
     <h2>${esc(vals.length > 28 ? s.pkg.split(" (")[0] : vals)}: ${s.refs.length} part${s.refs.length > 1 ? "s" : ""}</h2>
     <div class="refs">${s.refs.join("  ")}</div>
     ${draw(s)}
     <div class="legend">Red: this step · blue: placed · grey-green: still to do · yellow dot: pin 1 / cathode / +</div>
     <div class="nav"><button id="prev">← Back</button><button class="primary" id="tick">${done[cur] ? "Untick" : "Placed, next →"}</button>
     <span class="prog">${n} of ${D.steps.length} done</span></div>
     <dl>${rows.map(([k, v]) => `<dt>${k}</dt><dd>${md(v)}</dd>`).join("")}</dl>`;
  document.getElementById("prev").onclick = () => go(cur - 1);
  document.getElementById("tick").onclick = () => { toggle(cur); if (done[cur]) go(cur + 1); };
}
function renderList() {
  let h = "", side = "";
  D.steps.forEach((s, i) => {
    if (s.side !== side) { side = s.side; h += `<div class="side">${esc(D.sides[side])}</div>`; }
    h += `<div class="step${i === cur ? " cur" : ""}${done[i] ? " done" : ""}" data-i="${i}">
      <input type="checkbox" ${done[i] ? "checked" : ""} data-c="${i}" aria-label="Placed">
      <span class="n">${i + 1}</span><div><div class="t">${esc(s.refs.join(", "))}</div>
      <div class="r">${esc(s.bag.split(" (")[0])} · ${esc(s.pkg.split(" (")[0])}</div></div></div>`;
  });
  const list = document.getElementById("list");
  list.innerHTML = h;
  list.querySelectorAll(".step").forEach(el => el.onclick = e => {
    const i = +el.dataset.i;
    if (e.target.dataset.c !== undefined) { toggle(i); return; }
    go(i);
  });
}
function toggle(i) { done[i] = !done[i]; if (!done[i]) delete done[i]; save(); renderList(); renderCard(); }
function go(i) {
  cur = Math.max(0, Math.min(D.steps.length - 1, i));
  renderList(); renderCard();
  const el = document.querySelector(`.step[data-i="${cur}"]`);
  if (el) el.scrollIntoView({block: "nearest"});
}
document.addEventListener("keydown", e => {
  if (e.key === "ArrowRight" || e.key === "ArrowDown") { go(cur + 1); e.preventDefault(); }
  if (e.key === "ArrowLeft" || e.key === "ArrowUp") { go(cur - 1); e.preventDefault(); }
  if (e.key === " ") { toggle(cur); e.preventDefault(); }
});
const hashStep = parseInt((location.hash.match(/^#(\d+)$/) || [])[1], 10);
const firstOpen = D.steps.findIndex((_, i) => !done[i]);
go(hashStep ? hashStep - 1 : firstOpen < 0 ? 0 : firstOpen);
</script>
</body>
</html>
"""

if __name__ == "__main__":
    main()
