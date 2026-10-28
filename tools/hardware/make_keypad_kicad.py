#!/usr/bin/env python3
"""Writes the keypad boards as KiCad projects under hardware/keypad/:

    key_column  8 Kailh Box (MX-pin) switches in hotswap sockets, one column,
                an RGB LED under each, a TCA9534A reading the keys
    key_upper   the same with 3 keys, for the top of the pole's bottom segment
    key_hub     plugs into the power board's keypad port and fans out to
                four key boards, buffering the LED data to 5 V

    KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
    $KPY tools/hardware/make_keypad_kicad.py [key_column key_upper key_hub]

Run it with KiCad's own Python (the boards are built with pcbnew). Symbols
and footprints are KiCad's own, except the hotswap socket, drawn here into
hardware/keypad/nibbles_keys.pretty. Every part is placed; nothing is
routed. Regenerating overwrites the project: once a board has been edited
in KiCad, edit it there (or by script on a copy), not by rerunning this.

Every key board and hub port uses the same JST-GH 8 pinout:
1 GND, 2 +3V3, 3 SDA, 4 SCL, 5 INT, 6 +5V, 7 LED data in, 8 LED data out.
Pins 1-6 match the power board's GH6 keypad ports (J9, J11).
"""
import json
import math
import os
import sys
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from kicad_sexpr import KICAD_FOOTPRINTS, Q, dump, kid, kids, lib_symbol, pins  # noqa: E402

REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(REPO, "hardware", "keypad")
NS = uuid.UUID("0b7e5a8e-3c1f-4d47-9a52-1f3d2c6e8a10")

PITCH = 16.0        # key pitch: Kailh Box housings are 15.6 mm square, plate cutouts 14 mm (2 mm web)
WIDTH = 18.0        # key board width: the hotswap sockets' pads span 16.7 mm
KEY1_Y = 13.5       # first key's centre from the top edge (room for a mounting hole)


def uid(*k):
    return str(uuid.uuid5(NS, "/".join(str(x) for x in k)))


def f(v):
    s = ("%.4f" % v).rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


# ------------------------------------------------------------------ parts
class Part:
    def __init__(self, ref, sym, value, fp, nets, sch, pcb, lcsc="", mpn="", bom=True, nc=()):
        self.ref, self.sym, self.value, self.fp, self.nets = ref, sym, value, fp, nets
        self.sch, self.pcb, self.lcsc, self.mpn, self.bom, self.nc = sch, pcb, lcsc, mpn, bom, set(nc)


GH8 = "Connector_JST:JST_GH_BM08B-GHS-TBT_1x08-1MP_P1.25mm_Vertical"
C0603 = "Capacitor_SMD:C_0603_1608Metric"
C0805 = "Capacitor_SMD:C_0805_2012Metric"
R0603 = "Resistor_SMD:R_0603_1608Metric"
HOTSWAP = "nibbles_keys:SW_Kailh_MX_Hotswap"
LEDFP = "LED_SMD:LED_SK6812MINI-E_3.2x2.8mm_P1.5mm_ReverseMount"
JP3 = "Jumper:SolderJumper-3_P1.3mm_Bridged12_RoundedPad1.0x1.5mm_NumberLabels"
JP2 = "Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm"


def port_nets(din, dout):
    return {"1": "GND", "2": "+3V3", "3": "SDA", "4": "SCL", "5": "INT", "6": "+5V", "7": din, "8": dout}


def key_board(n, name, title):
    """A column of n keys. PCB coordinates: x across (0 = centre), y down from the top edge."""
    yk = [KEY1_Y + PITCH * i for i in range(n)]
    y0 = yk[-1] + 8.4            # electronics block starts below the last key's LED
    length = y0 + 22.3
    P = []
    # schematic: keys and LEDs in two rows of columns, the expander and connector on the left
    for i in range(n):
        k = i + 1
        sx = 38.1 + 40.64 * (i % 4)
        sy = 127.0 + 76.2 * (i // 4)
        P.append(Part(f"SW{k}", "Switch:SW_Push", "Kailh Box Navy", HOTSWAP, {"1": "GND", "2": f"KEY{k}"},
                      (sx, sy), (0, yk[i], 0, "F"), lcsc="C41430893", mpn="Kailh CPG151101S11 socket"))
        P.append(Part(f"D{k}", "LED:SK6812MINI-E", "SK6812MINI-E", LEDFP,
                      {"1": "GND", "2": "LED_DIN" if k == 1 else f"LED{k - 1}", "3": "+5V",
                       "4": "LED_LAST" if k == n else f"LED{k}"},
                      (sx, sy + 25.4), (0, yk[i] + 5.08, 180, "B"), lcsc="C5149201", mpn="SK6812MINI-E"))
        # each LED's 100 nF, beside it
        P.append(Part(f"C{k + 10}", "Device:C", "100nF", C0603, {"1": "+5V", "2": "GND"},
                      (sx, sy + 55.88), (-5.9, yk[i] + 5.6, 90, "B"), lcsc="C14663"))
    P += [
        Part("J1", "Connector_Generic:Conn_01x08", "Hub (JST-GH 8)", GH8, port_nets("LED_DIN", "LED_DOUT"),
             (38.1, 63.5), (0, y0 + 18.6, 0, "B"), lcsc="C133062", mpn="BM08B-GHS-TBT"),
        Part("U1", "Interface_Expansion:TCA9534", "TCA9534APWR", "Package_SO:TSSOP-16_4.4x5mm_P0.65mm",
             {"1": "ADDR0", "2": "ADDR1", "3": "ADDR2", "4": "KEY1", "5": "KEY2", "6": "KEY3", "7": "KEY4",
              "8": "GND", "9": "KEY5", "10": "KEY6", "11": "KEY7", "12": "KEY8", "13": "INT", "14": "SCL",
              "15": "SDA", "16": "+3V3"},
             (76.2, 63.5), (0, y0 + 4.4, 90, "B"), lcsc="C206010", mpn="TCA9534APWR"),
        Part("RN1", "Device:R_Pack04", "10k x4", "Resistor_SMD:R_Array_Convex_4x0603",
             {"1": "KEY1", "2": "KEY2", "3": "KEY3", "4": "KEY4", "5": "+3V3", "6": "+3V3", "7": "+3V3", "8": "+3V3"},
             (106.68, 63.5), (-6.3, y0 + 8.0, 0, "B"), lcsc="C29718", mpn="4D03WGJ0103T5E"),
        Part("RN2", "Device:R_Pack04", "10k x4", "Resistor_SMD:R_Array_Convex_4x0603",
             {"1": "KEY5", "2": "KEY6", "3": "KEY7", "4": "KEY8", "5": "+3V3", "6": "+3V3", "7": "+3V3", "8": "+3V3"},
             (127.0, 63.5), (6.3, y0 + 8.0, 0, "B"), lcsc="C29718", mpn="4D03WGJ0103T5E"),
        Part("C1", "Device:C", "100nF", C0603, {"1": "+3V3", "2": "GND"}, (142.24, 63.5), (-2.6, y0 + 10.6, 0, "B"),
             lcsc="C14663"),
        Part("C2", "Device:C", "10uF", C0805, {"1": "+5V", "2": "GND"}, (152.4, 63.5), (4.8, y0 + 11.0, 0, "B"),
             lcsc="C15850"),
        Part("R1", "Device:R", "100R", R0603, {"1": "LED_LAST", "2": "LED_DOUT"}, (162.56, 63.5),
             (1.0, y0 + 10.6, 0, "B"), lcsc="C22775"),
    ]
    for a in range(3):
        P.append(Part(f"JP{a + 1}", "Jumper:SolderJumper_3_Bridged12", f"A{a}", JP3,
                      {"1": "GND", "2": f"ADDR{a}", "3": "+3V3"}, (38.1 + 33.02 * a, 99.06),
                      (-5.0 + 5.0 * a, y0 + 13.4, 0, "B"), bom=False))
    P += [Part("H1", "Mechanical:MountingHole", "M2", "MountingHole:MountingHole_2.2mm_M2", {},
               (190.5, 99.06), (-6.5, 2.8, 0, "F"), bom=False),
          Part("H2", "Mechanical:MountingHole", "M2", "MountingHole:MountingHole_2.2mm_M2", {},
               (203.2, 99.06), (6.5, 2.8, 0, "F"), bom=False),
          Part("H3", "Mechanical:MountingHole", "M2", "MountingHole:MountingHole_2.2mm_M2", {},
               (215.9, 99.06), (-6.5, y0 + 2.6, 0, "F"), bom=False),
          Part("H4", "Mechanical:MountingHole", "M2", "MountingHole:MountingHole_2.2mm_M2", {},
               (228.6, 99.06), (6.5, y0 + 2.6, 0, "F"), bom=False)]
    notes = [
        f"{title}: {n} Kailh Box Navy switches (MX pins) in Kailh hotswap sockets on the back, {PITCH:g} mm pitch,",
        "an SK6812MINI-E under each switch's LED window (mounted on the back, shining through), and a TCA9534A",
        "(I2C 0x38-0x3F, set by JP1-JP3; default 0x38) reading the keys: pressed = low, INT on any change.",
        "Address: JP1-JP3 are A0-A2, bridged 1-2 (0) as made; cut 1-2 and bridge 2-3 for a 1. Give each board its own.",
        "J1 to the keypad hub (JST-GH 8): 1 GND, 2 +3V3, 3 SDA, 4 SCL, 5 INT, 6 +5V, 7 LED data in, 8 LED data out.",
        "The 3.49 drives the LED data (GPIO 43 via the hub's 5 V buffer); the chain runs through every board in turn.",
    ]
    if n < 8:
        notes.append(f"P{n}-P7 are unused, held high by RN1/RN2.")
    return dict(name=name, title=title, parts=P, notes=notes, outline=("rect", -WIDTH / 2, 0, WIDTH / 2, length),
                key_y=yk)


def hub_board():
    """Two rows of JST-GH 8: J1 (in), J2, J3 on top, J4, J5 below; 48 x 24 mm."""
    P = [
        Part("J1", "Connector_Generic:Conn_01x08", "From the power board (J9 + GPIO 43)", GH8,
             {"1": "GND", "2": "+3V3", "3": "SDA", "4": "SCL", "5": "INT", "6": "+5V", "7": "LED_DATA_3V3"},
             (38.1, 63.5), (8.0, 4.6, 0, "F"), lcsc="C133062", mpn="BM08B-GHS-TBT", nc={"8"}),
        Part("U1", "74xGxx:74AHCT1G125", "SN74AHCT1G125DBVR", "Package_TO_SOT_SMD:SOT-23-5",
             {"1": "GND", "2": "LED_DATA_3V3", "3": "GND", "4": "LED_BUF", "5": "+5V"},
             (88.9, 63.5), (36.5, 19.0, 0, "F"), lcsc="C7484", mpn="SN74AHCT1G125DBVR"),
        Part("R1", "Device:R", "100k", R0603, {"1": "LED_DATA_3V3", "2": "GND"}, (63.5, 101.6),
             (40.5, 20.2, 0, "F"), lcsc="C25803"),
        Part("R2", "Device:R", "100R", R0603, {"1": "LED_BUF", "2": "LED_CH1"}, (114.3, 63.5),
             (40.5, 18.2, 0, "F"), lcsc="C22775"),
        Part("C1", "Device:C", "100nF", C0603, {"1": "+5V", "2": "GND"}, (76.2, 101.6), (44.5, 19.2, 90, "F"),
             lcsc="C14663"),
        Part("C2", "Device:C", "22uF", C0805, {"1": "+5V", "2": "GND"}, (88.9, 101.6), (14.0, 12.0, 0, "F"),
             lcsc="C45783"),
        Part("C3", "Device:C", "22uF", C0805, {"1": "+5V", "2": "GND"}, (101.6, 101.6), (18.5, 12.0, 0, "F"),
             lcsc="C45783"),
        Part("C4", "Device:C", "100nF", C0603, {"1": "+3V3", "2": "GND"}, (114.3, 101.6), (22.5, 12.0, 0, "F"),
             lcsc="C14663"),
        Part("H1", "Mechanical:MountingHole", "M2.5", "MountingHole:MountingHole_2.7mm_M2.5", {},
             (203.2, 101.6), (3.5, 12.0, 0, "F"), bom=False),
        Part("H2", "Mechanical:MountingHole", "M2.5", "MountingHole:MountingHole_2.7mm_M2.5", {},
             (215.9, 101.6), (44.5, 12.0, 0, "F"), bom=False),
    ]
    spots = ((24.0, 4.6, 0), (40.0, 4.6, 0), (8.0, 19.4, 180), (24.0, 19.4, 180))
    for k in range(4):
        x, y, rot = spots[k]
        P.append(Part(f"J{k + 2}", "Connector_Generic:Conn_01x08", f"Key board {k + 1}", GH8,
                      port_nets(f"LED_CH{k + 1}", f"LED_CH{k + 2}"), (139.7 + 33.02 * k, 63.5), (x, y, rot, "F"),
                      lcsc="C133062", mpn="BM08B-GHS-TBT"))
        P.append(Part(f"JP{k + 1}", "Jumper:SolderJumper_2_Open", f"skip port {k + 1}", JP2,
                      {"1": f"LED_CH{k + 1}", "2": f"LED_CH{k + 2}"}, (139.7 + 33.02 * k, 101.6),
                      (26.0 + 4.0 * k, 12.0, 0, "F"), bom=False))
    return dict(name="key_hub", title="Keypad hub", parts=P, outline=("rect", 0, 0, 48.0, 24.0), notes=[
        "Keypad hub: J1 takes the power board's keypad port (J9, JST-GH 6 on pins 1-6) plus one wire from the 3.49's",
        "GPIO 43 (the power board's J6 pad B5) on pin 7. J2-J5 go to key boards (same GH8 pinout as theirs).",
        "U1 buffers the LED data to 5 V (SK6812 needs 0.7 VDD). It runs J2 in -> J2 out -> J3 in ... J5 out:",
        "fill the ports in order, or bridge a port's JP to skip it. R1 holds the data low with the cable out.",
        "I2C pull-ups are on the power board (4.7k); INT is open drain, pulled up there (10k).",
    ])


# --------------------------------------------------------------- footprints
def hotswap_footprint():
    """Kailh CPG151101S11 MX hotswap socket on the back, MX switch holes (3- and 5-pin) through the board."""
    def pad(n, x, y):
        return (f'\t(pad "{n}" smd roundrect (at {f(x)} {f(y)}) (size 2.55 2.5) (layers "B.Cu" "B.Paste" "B.Mask") '
                f'(roundrect_rratio 0.1) (uuid "{uid("hs", n)}"))')

    def hole(x, y, d):
        return (f'\t(pad "" np_thru_hole circle (at {f(x)} {f(y)}) (size {f(d)} {f(d)}) (drill {f(d)}) '
                f'(layers "*.Cu" "*.Mask") (uuid "{uid("hsh", x, y)}"))')

    def line(x1, y1, x2, y2, layer, w):
        return (f'\t(fp_line (start {f(x1)} {f(y1)}) (end {f(x2)} {f(y2)}) (stroke (width {f(w)}) (type solid)) '
                f'(layer "{layer}") (uuid "{uid("hsl", x1, y1, x2, y2, layer)}"))')

    out = ['(footprint "SW_Kailh_MX_Hotswap"', '\t(version 20241229)', '\t(generator "nibbles")',
           '\t(layer "F.Cu")',
           '\t(descr "Cherry MX / Kailh Box switch in a Kailh CPG151101S11 hotswap socket on the back. '
           'Switch on the front; socket pads on B.Cu")',
           '\t(tags "MX hotswap Kailh keyswitch")',
           '\t(property "Reference" "REF**" (at 0 -8.6 0) (layer "F.SilkS") (effects (font (size 1 1) (thickness 0.15))))',
           '\t(property "Value" "SW_Kailh_MX_Hotswap" (at 0 8.6 0) (layer "F.Fab") '
           '(effects (font (size 1 1) (thickness 0.15))))',
           '\t(attr smd)']
    # switch outline (front) and the 14 mm plate cutout, for reference
    for (x1, y1, x2, y2) in ((-7.8, -7.8, 7.8, -7.8), (7.8, -7.8, 7.8, 7.8), (7.8, 7.8, -7.8, 7.8), (-7.8, 7.8, -7.8, -7.8)):
        out.append(line(x1, y1, x2, y2, "F.Fab", 0.1))
    for (x1, y1, x2, y2) in ((-7, -7, 7, -7), (7, -7, 7, 7), (7, 7, -7, 7), (-7, 7, -7, -7)):
        out.append(line(x1, y1, x2, y2, "Dwgs.User", 0.1))
    # socket body on the back (silk and courtyard)
    body = [(-5.0, -7.1), (4.8, -7.1), (4.8, -3.6), (-1.2, -3.6), (-1.2, -0.9), (-5.0, -0.9)]
    for a, b in zip(body, body[1:] + body[:1]):
        out.append(line(a[0], a[1], b[0], b[1], "B.SilkS", 0.12))
    for a, b in zip(((-8.6, -7.4), (7.4, -7.4), (7.4, -0.6), (-8.6, -0.6)),
                    ((7.4, -7.4), (7.4, -0.6), (-8.6, -0.6), (-8.6, -7.4))):
        out.append(line(a[0], a[1], b[0], b[1], "B.CrtYd", 0.05))
    out.append(line(-7.95, -7.95, 7.95, -7.95, "F.CrtYd", 0.05))
    out.append(line(7.95, -7.95, 7.95, 7.95, "F.CrtYd", 0.05))
    out.append(line(7.95, 7.95, -7.95, 7.95, "F.CrtYd", 0.05))
    out.append(line(-7.95, 7.95, -7.95, -7.95, "F.CrtYd", 0.05))
    out += [hole(0, 0, 4.0), hole(-5.08, 0, 1.75), hole(5.08, 0, 1.75), hole(-3.81, -2.54, 3.0), hole(2.54, -5.08, 3.0),
            pad("1", -7.085, -2.54), pad("2", 5.842, -5.08)]
    out.append(')')
    return "\n".join(out) + "\n"


def write_footprints():
    d = os.path.join(OUT, "nibbles_keys.pretty")
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "SW_Kailh_MX_Hotswap.kicad_mod"), "w").write(hotswap_footprint())


# --------------------------------------------------------------- schematic
def sheet_pin(part, sym, num):
    px, py = pins(sym)[num][:2]
    return part.sch[0] + px, part.sch[1] - py


def outward(sym, num):
    a = (pins(sym)[num][2] + 180) % 360
    return {0: (1, 0), 90: (0, -1), 180: (-1, 0), 270: (0, 1)}[int(a)], int(a)


POWER = {"GND": "power:GND", "+3V3": "power:+3V3", "+5V": "power:+5V"}


def write_schematic(B, d, root):
    syms = {}
    for p in B["parts"]:
        syms.setdefault(p.sym, lib_symbol(p.sym))
    for lid in list(POWER.values()) + ["power:PWR_FLAG"]:
        syms.setdefault(lid, lib_symbol(lid))
    items = []
    n = [0]

    def nid(*k):
        n[0] += 1
        return Q(uid(B["name"], *k, n[0]))

    def place_symbol(lib_id, ref, value, x, y, rot, fp="", lcsc="", mpn="", bom=True, power=False):
        sym = syms[lib_id]
        P = pins(sym)
        props = [["property", Q("Reference"), Q(ref)], ["property", Q("Value"), Q(value)],
                 ["property", Q("Footprint"), Q(fp)], ["property", Q("Datasheet"), Q("")]]
        if lcsc:
            props.append(["property", Q("LCSC"), Q(lcsc)])
        if mpn:
            props.append(["property", Q("MPN"), Q(mpn)])
        xs = [v[0] for v in P.values()] or [0]
        ys = [v[1] for v in P.values()] or [0]
        for pr in props:
            nm = pr[1]
            if power:
                at = (x, y + (3.81 if lib_id == "power:GND" and rot == 0 else -3.81), 0)
                hide = nm != "Value"
            elif nm in ("Reference", "Value") and P and max(ys) <= 0 and min(xs) < 0 < max(xs):
                # side pins only (switches, jumpers): text above and below, clear of the pins' labels
                at, hide = (x - 2.54, y - 3.81 if nm == "Reference" else y + 5.08, 0), False
            elif nm == "Reference":
                at, hide = (x + max(xs) + 1.27, y - max(ys) - 1.27, 0), False
            elif nm == "Value":
                at, hide = (x + max(xs) + 1.27, y - min(ys) + 1.27 + (1.27 if max(xs) > 0 else 0), 0), False
            else:
                at, hide = (x, y, 0), True
            pr += [["at", f(at[0]), f(at[1]), f(at[2])], ["effects", ["font", ["size", "1.27", "1.27"]]] +
                   ([["justify", "left"]] if not power else [])]
            if hide:
                pr.append(["hide", "yes"])
        s = ["symbol", ["lib_id", Q(lib_id)], ["at", f(x), f(y), str(rot)], ["unit", "1"],
             ["exclude_from_sim", "no"], ["in_bom", "yes" if bom and not power else "no"],
             ["on_board", "no" if power else "yes"], ["dnp", "no"], ["uuid", Q(uid(B["name"], "sym", ref))]] + props
        s += [["pin", Q(num), ["uuid", Q(uid(B["name"], "pin", ref, num))]] for num in P]
        s.append(["instances", ["project", Q(B["name"]), ["path", Q("/" + root), ["reference", Q(ref)],
                                                                 ["unit", "1"]]]])
        items.append(s)

    def wire(a, b):
        items.append(["wire", ["pts", ["xy", f(a[0]), f(a[1])], ["xy", f(b[0]), f(b[1])]],
                      ["stroke", ["width", "0"], ["type", "default"]], ["uuid", nid("w")]])

    pwr = [0]

    def power(net, x, y, direction):
        rot = {(0, 1): 0, (1, 0): 90, (0, -1): 180, (-1, 0): 270}[direction]
        if net != "GND":
            rot = (rot + 180) % 360
        pwr[0] += 1
        place_symbol(POWER[net], f"#PWR{pwr[0]:03d}", net, x, y, rot, power=True)

    def label(net, x, y, a):
        # global labels, so nets are plain names (SDA, not /SDA) on the board too
        just = "right" if a in (180, 270) else "left"
        items.append(["global_label", Q(net), ["shape", "passive"], ["at", f(x), f(y), str(a)],
                      ["fields_autoplaced", "yes"], ["effects", ["font", ["size", "1.27", "1.27"]], ["justify", just]],
                      ["uuid", nid("l")],
                      ["property", Q("Intersheetrefs"), Q("${INTERSHEET_REFS}"), ["at", f(x), f(y), "0"],
                       ["effects", ["font", ["size", "1.27", "1.27"]], ["hide", "yes"]]]])

    for p in B["parts"]:
        sym = syms[p.sym]
        place_symbol(p.sym, p.ref, p.value, p.sch[0], p.sch[1], 0, p.fp, p.lcsc, p.mpn, p.bom)
        ends = {}   # (net, direction) -> stub ends, so a row of pins on one supply shares one symbol
        for num in pins(sym):
            x, y = sheet_pin(p, sym, num)
            if num in p.nc or num not in p.nets:
                assert num in p.nc or not p.nets, f"{p.ref} pin {num} has no net"
                items.append(["no_connect", ["at", f(x), f(y)], ["uuid", nid("nc")]])
                continue
            (dx, dy), a = outward(sym, num)
            e = (x + dx * 2.54, y + dy * 2.54)
            wire((x, y), e)
            net = p.nets[num]
            if net in POWER and dy:   # supplies on vertical pins: power symbols, one per row
                ends.setdefault((net, (dx, dy)), []).append(e)
            else:                     # everything else (supplies on side pins too): a label
                label(net, e[0], e[1], a)
        for (net, dirn), es in ends.items():
            es.sort()
            for a_, b_ in zip(es, es[1:]):
                wire(a_, b_)
            power(net, es[0][0], es[0][1], dirn)
    # power flags: the supplies come in on connectors
    fx = 25.4
    for net in POWER:
        y = 279.4
        power(net, fx, y, (0, 1) if net == "GND" else (0, -1))
        wire((fx, y), (fx + 7.62, y))
        pwr[0] += 1
        place_symbol("power:PWR_FLAG", f"#FLG{pwr[0]:03d}", "PWR_FLAG", fx + 7.62, y, 0, power=True)
        fx += 25.4
    for i, line in enumerate(B["notes"]):
        items.append(["text", Q(line), ["exclude_from_sim", "no"], ["at", "25.4", f(15.24 + 3.81 * i), "0"],
                      ["effects", ["font", ["size", "2", "2"]], ["justify", "left", "bottom"]], ["uuid", nid("t")]])
    root_node = ["kicad_sch", ["version", "20250114"], ["generator", Q("nibbles")], ["generator_version", Q("9.0")],
                 ["uuid", Q(root)], ["paper", Q("A3")],
                 ["title_block", ["title", Q(B["title"])], ["company", Q("Nibbles")],
                  ["comment", "1", Q("Generated by tools/hardware/make_keypad_kicad.py; see hardware/keypad/README.md")]],
                 ["lib_symbols"] + list(syms.values())] + items + \
                [["sheet_instances", ["path", Q("/"), ["page", Q("1")]]], ["embedded_fonts", "no"]]
    open(os.path.join(d, B["name"] + ".kicad_sch"), "w").write(dump(root_node) + "\n")


# ------------------------------------------------------------------- board
ORIGIN = (100.0, 60.0)


def write_board(B, d, root):
    import pcbnew
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(2)
    mm = pcbnew.FromMM
    nets = {}

    def net(name):
        if name not in nets:
            nets[name] = pcbnew.NETINFO_ITEM(board, name)
            board.Add(nets[name])
        return nets[name]

    for p in B["parts"]:
        lib, name = p.fp.split(":")
        folder = os.path.join(OUT, "nibbles_keys.pretty") if lib == "nibbles_keys" else \
            os.path.join(KICAD_FOOTPRINTS, lib + ".pretty")
        fp = pcbnew.FootprintLoad(folder, name)
        assert fp, p.fp
        fp.SetFPIDAsString(p.fp)
        fp.SetReference(p.ref)
        fp.SetValue(p.value)
        for k, v in (("LCSC", p.lcsc), ("MPN", p.mpn)):
            if v:
                fp.SetField(k, v)
        for fl in fp.GetFields():
            if fl.GetName() in ("LCSC", "MPN"):
                fl.SetVisible(False)
                fl.SetLayer(pcbnew.F_Fab)
        fp.SetPath(pcbnew.KIID_PATH(f"/{uid(B['name'], 'sym', p.ref)}"))
        fp.SetSheetname("/")
        fp.SetSheetfile(B["name"] + ".kicad_sch")
        if not p.bom:
            fp.SetExcludedFromBOM(True)
        names = {k: v[4] for k, v in pins(lib_symbol(p.sym)).items()}
        for pad in fp.Pads():
            num = pad.GetNumber()
            nn = p.nets.get(num) or (f"unconnected-({p.ref}-{names[num]}-Pad{num})" if num in p.nc else None)
            if nn:
                pad.SetNet(net(nn))
        x, y, rot, side = p.pcb
        board.Add(fp)   # before flipping: Flip needs the board
        fp.SetPosition(pcbnew.VECTOR2I(mm(ORIGIN[0] + x), mm(ORIGIN[1] + y)))
        if side == "B":
            fp.Flip(fp.GetPosition(), pcbnew.FLIP_DIRECTION_LEFT_RIGHT)
        fp.SetOrientationDegrees(rot)

    kind, x0, y0, x1, y1 = B["outline"]
    pts = [(x0, y0), (x1, y0), (x1, y1), (x0, y1)]
    r = 1.0   # rounded corners
    for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        dx, dy = (bx - ax), (by - ay)
        L = math.hypot(dx, dy)
        ux, uy = dx / L, dy / L
        seg.SetStart(pcbnew.VECTOR2I(mm(ORIGIN[0] + ax + ux * r), mm(ORIGIN[1] + ay + uy * r)))
        seg.SetEnd(pcbnew.VECTOR2I(mm(ORIGIN[0] + bx - ux * r), mm(ORIGIN[1] + by - uy * r)))
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(mm(0.05))
        board.Add(seg)
        arc = pcbnew.PCB_SHAPE(board)
        arc.SetShape(pcbnew.SHAPE_T_ARC)
        # corner at b: from (b - u r) around to (b + v r), centre b - u r + v r
        nx, ny = pts[(pts.index((bx, by)) + 1) % 4]
        vx, vy = (nx - bx), (ny - by)
        Lv = math.hypot(vx, vy)
        vx, vy = vx / Lv, vy / Lv
        cx, cy = bx - ux * r + vx * r, by - uy * r + vy * r
        arc.SetCenter(pcbnew.VECTOR2I(mm(ORIGIN[0] + cx), mm(ORIGIN[1] + cy)))
        arc.SetStart(pcbnew.VECTOR2I(mm(ORIGIN[0] + bx - ux * r), mm(ORIGIN[1] + by - uy * r)))
        arc.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(90, pcbnew.DEGREES_T), True)
        arc.SetLayer(pcbnew.Edge_Cuts)
        arc.SetWidth(mm(0.05))
        board.Add(arc)
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(net("GND"))
        z.SetZoneName("GND")
        z.SetMinThickness(mm(0.2))
        z.SetLocalClearance(mm(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
        ol = z.Outline()
        ol.NewOutline()
        for px, py in pts:
            ol.Append(mm(ORIGIN[0] + px), mm(ORIGIN[1] + py))
        board.Add(z)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    path = os.path.join(d, B["name"] + ".kicad_pcb")
    board.SetFileName(path)
    pcbnew.SaveBoard(path, board)
    return len(B["parts"]), len(nets)


def write_project(B, d):
    pro = os.path.join(d, B["name"] + ".kicad_pro")
    data = json.load(open(pro)) if os.path.exists(pro) else {"meta": {"filename": B["name"] + ".kicad_pro", "version": 1}}
    rules = data.setdefault("board", {}).setdefault("design_settings", {}).setdefault("rules", {})
    # JLCPCB's standard 2-layer process, with margin; edge 0.2 mm for the SK6812MINI-E's own cutout
    rules.update({"min_clearance": 0.15, "min_track_width": 0.15, "min_via_diameter": 0.5,
                  "min_through_hole_diameter": 0.3, "min_copper_edge_clearance": 0.2, "min_hole_clearance": 0.25,
                  "min_hole_to_hole": 0.25})
    ns = data.setdefault("net_settings", {})
    ns["classes"] = [{"name": "Default", "clearance": 0.15, "track_width": 0.2, "via_diameter": 0.6, "via_drill": 0.3},
                     {"name": "Supply", "clearance": 0.2, "track_width": 0.5, "via_diameter": 0.8, "via_drill": 0.4}]
    ns["netclass_patterns"] = [{"netclass": "Supply", "pattern": "+5V"}, {"netclass": "Supply", "pattern": "+3V3"}]
    json.dump(data, open(pro, "w"), indent=2)
    open(os.path.join(d, "fp-lib-table"), "w").write(
        '(fp_lib_table\n\t(version 7)\n\t(lib (name "nibbles_keys") (type "KiCad") '
        '(uri "${KIPRJMOD}/../nibbles_keys.pretty") (options "") (descr "Nibbles keypad footprints"))\n)\n')


def build(B):
    d = os.path.join(OUT, B["name"])
    os.makedirs(d, exist_ok=True)
    root = uid(B["name"], "root")
    write_project(B, d)
    write_schematic(B, d, root)
    np_, nn = write_board(B, d, root)
    write_project(B, d)   # saving the board rewrites the project file
    print(f"{B['name']}: {np_} parts, {nn} nets")


BOARDS = {"key_column": lambda: key_board(8, "key_column", "Key column (8 keys)"),
          "key_upper": lambda: key_board(3, "key_upper", "Upper keypad (3 keys)"),
          "key_hub": hub_board}

if __name__ == "__main__":
    write_footprints()
    for name in sys.argv[1:] or list(BOARDS):
        build(BOARDS[name]())
