#!/usr/bin/env python3
"""Writes the pole-base boards' schematics as EAGLE .sch files, which
Fusion's Electronics workspace opens (File > Open, or upload to a project).

    python3 tools/hardware/make_power_board_sch.py
        writes hardware/power_board/fusion/nibbles_power_board.sch
           and hardware/charger_board/fusion/nibbles_charger_board.sch

Two boards, joined by cables: the power board (LEMO input, lighting path,
5 V for the base, I/O expander, ports) and the charger board (BQ25798),
which plugs into the power board's charger port so it can be swapped out on
its own (plan B: last year's USB-C charger modules on the same port). Net
names match across the cables. The design follows docs/power.md (see each
board's README.md for the part choices, the datasheet pin numbers and what
still needs checking).

Parts are symbol-only (no footprints): in Fusion, replace each with a library
part that has one (Replace keeps the connections where the pin names match).
Nets are joined by name: every pin has a short wire ending in a net label, and
all labels with the same name are one net, across all sheets.
"""
import os
from xml.sax.saxutils import escape

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
BOARDS = {  # board -> (folder under hardware/, file name, title)
    "power": ("power_board", "nibbles_power_board", "Nibbles pole-base power board"),
    "charger": ("charger_board", "nibbles_charger_board", "Nibbles charger board"),
}


def out_path(board):
    folder, name, _ = BOARDS[board]
    return os.path.join(REPO, "hardware", folder, "fusion", name + ".sch")

G = 2.54  # grid, mm


# ---------------------------------------------------------------- symbols
class Sym:
    """A symbol: pins as (name, x, y, rot); rot R0 = pin points left out of
    the body (connection point on the left), R180 = right, R90 = down, R270 = up."""
    def __init__(self, name, pins, draw, label_xy):
        self.name, self.pins, self.draw, self.label_xy = name, pins, draw, label_xy

    def pin(self, n):
        for p in self.pins:
            if p[0] == n:
                return p
        raise KeyError(f"{self.name} has no pin {n}")


def ic_symbol(name, left, right):
    longest = max([len(p) for p in left + right] + [6])
    w = max(15.24, round((longest * 2 * 1.4 + 6) / (2 * G)) * 2 * G)  # an even number of grid steps
    rows = max(len(left), len(right))
    top = (rows // 2) * G  # pins on the 0.1 inch grid
    pins = []
    for i, n in enumerate(left):
        pins.append((n, -w / 2 - G, top - i * G, "R0"))
    for i, n in enumerate(right):
        pins.append((n, w / 2 + G, top - i * G, "R180"))
    x0, y0, x1, y1 = -w / 2, top - rows * G, w / 2, top + G
    draw = [f'<wire x1="{x0}" y1="{y0}" x2="{x1}" y2="{y0}" width="0.254" layer="94"/>',
            f'<wire x1="{x1}" y1="{y0}" x2="{x1}" y2="{y1}" width="0.254" layer="94"/>',
            f'<wire x1="{x1}" y1="{y1}" x2="{x0}" y2="{y1}" width="0.254" layer="94"/>',
            f'<wire x1="{x0}" y1="{y1}" x2="{x0}" y2="{y0}" width="0.254" layer="94"/>',
            f'<text x="{x0}" y="{y1 + 1.27}" size="1.778" layer="95">&gt;NAME</text>',
            f'<text x="{x0}" y="{y0 - 3.048}" size="1.778" layer="96">&gt;VALUE</text>']
    return Sym(name, pins, draw, None)


def two_pin(name, a="1", b="2", body="rect"):
    pins = [(a, -5.08, 0, "R0"), (b, 5.08, 0, "R180")]
    if body == "rect":
        draw = ['<wire x1="-2.54" y1="-1.016" x2="2.54" y2="-1.016" width="0.254" layer="94"/>',
                '<wire x1="2.54" y1="-1.016" x2="2.54" y2="1.016" width="0.254" layer="94"/>',
                '<wire x1="2.54" y1="1.016" x2="-2.54" y2="1.016" width="0.254" layer="94"/>',
                '<wire x1="-2.54" y1="1.016" x2="-2.54" y2="-1.016" width="0.254" layer="94"/>']
    elif body == "cap":
        draw = ['<wire x1="-2.54" y1="0" x2="-0.508" y2="0" width="0.1524" layer="94"/>',
                '<wire x1="0.508" y1="0" x2="2.54" y2="0" width="0.1524" layer="94"/>',
                '<rectangle x1="-0.762" y1="-1.524" x2="-0.254" y2="1.524" layer="94"/>',
                '<rectangle x1="0.254" y1="-1.524" x2="0.762" y2="1.524" layer="94"/>']
    elif body == "ind":
        draw = ['<wire x1="-2.54" y1="0" x2="2.54" y2="0" width="0.6096" layer="94"/>']
    elif body == "diode":  # a -> anode (left), b -> cathode (right)
        draw = ['<wire x1="-2.54" y1="0" x2="2.54" y2="0" width="0.1524" layer="94"/>',
                '<polygon width="0.1524" layer="94"><vertex x="-1.27" y="1.27"/><vertex x="1.27" y="0"/><vertex x="-1.27" y="-1.27"/></polygon>',
                '<wire x1="1.27" y1="1.27" x2="1.27" y2="-1.27" width="0.254" layer="94"/>']
    draw += ['<text x="-2.54" y="1.778" size="1.27" layer="95">&gt;NAME</text>',
             '<text x="-2.54" y="-3.048" size="1.27" layer="96">&gt;VALUE</text>']
    return Sym(name, pins, draw, None)


def nmos():
    pins = [("G", -5.08, 0, "R0"), ("D", 2.54, 5.08, "R270"), ("S", 2.54, -5.08, "R90")]
    draw = ['<wire x1="-2.54" y1="0" x2="-0.762" y2="0" width="0.1524" layer="94"/>',
            '<wire x1="-0.762" y1="-1.778" x2="-0.762" y2="1.778" width="0.254" layer="94"/>',
            '<wire x1="0" y1="-2.032" x2="0" y2="2.032" width="0.254" layer="94"/>',
            '<wire x1="0" y1="1.524" x2="2.54" y2="1.524" width="0.1524" layer="94"/>',
            '<wire x1="2.54" y1="1.524" x2="2.54" y2="2.54" width="0.1524" layer="94"/>',
            '<wire x1="0" y1="-1.524" x2="2.54" y2="-1.524" width="0.1524" layer="94"/>',
            '<wire x1="2.54" y1="-1.524" x2="2.54" y2="-2.54" width="0.1524" layer="94"/>',
            '<text x="3.81" y="0.508" size="1.27" layer="95">&gt;NAME</text>',
            '<text x="3.81" y="-1.778" size="1.27" layer="96">&gt;VALUE</text>']
    return Sym("NMOS", pins, draw, None)


def connector(name, pins_):
    n = len(pins_)
    h = n * G
    pins = [(p, -5.08, h / 2 - (i + 0.5) * G, "R0") for i, p in enumerate(pins_)]
    draw = [f'<wire x1="-2.54" y1="{-h / 2}" x2="2.54" y2="{-h / 2}" width="0.254" layer="94"/>',
            f'<wire x1="2.54" y1="{-h / 2}" x2="2.54" y2="{h / 2}" width="0.254" layer="94"/>',
            f'<wire x1="2.54" y1="{h / 2}" x2="-2.54" y2="{h / 2}" width="0.254" layer="94"/>',
            f'<wire x1="-2.54" y1="{h / 2}" x2="-2.54" y2="{-h / 2}" width="0.254" layer="94"/>',
            f'<text x="-2.54" y="{h / 2 + 1.27}" size="1.778" layer="95">&gt;NAME</text>',
            f'<text x="-2.54" y="{-h / 2 - 3.048}" size="1.778" layer="96">&gt;VALUE</text>']
    return Sym(name, pins, draw, None)


SYMBOLS = {
    "BQ25798": ic_symbol("BQ25798",
                         ["VAC1", "VAC2", "ACDRV1", "ACDRV2", "VBUS", "PMID", "D+", "D-", "ILIM_HIZ", "PROG",
                          "CE", "QON", "TS", "SDA", "SCL"],
                         ["SW1", "BTST1", "SW2", "BTST2", "SYS", "BAT", "BATP", "SDRV", "REGN", "STAT",
                          "INT", "GND"]),
    "CH224K": ic_symbol("CH224K", ["VDD", "VBUS", "CFG1", "CFG2", "CFG3"],
                        ["CC1", "CC2", "DP", "DM", "PG", "GND"]),
    "LM74700": ic_symbol("LM74700", ["ANODE", "EN", "VCAP"], ["CATHODE", "GATE", "GND"]),
    "TPS25982": ic_symbol("TPS25982",
                          ["IN", "EN/UVLO", "ITIMER", "ILIM", "RETRY_DLY", "NRETRY", "LDSTRT", "DVDT"],
                          ["OUT", "PG", "IMON", "GND"]),
    "INA228": ic_symbol("INA228", ["IN+", "IN-", "VBUS", "A0", "A1"], ["VS", "SDA", "SCL", "ALERT", "GND"]),
    "TPS62933": ic_symbol("TPS62933", ["VIN", "EN", "RT", "SS"], ["SW", "BST", "FB", "GND"]),
    "TCA9534": ic_symbol("TCA9534", ["VCC", "SDA", "SCL", "INT", "A0", "A1", "A2"],
                         ["P0", "P1", "P2", "P3", "P4", "P5", "P6", "P7", "GND"]),
    "R": two_pin("R"),
    "C": two_pin("C", body="cap"),
    "L": two_pin("L", body="ind"),
    "D": two_pin("D", "A", "C", body="diode"),
    "FUSE": two_pin("FUSE"),
    "NMOS": nmos(),
    "USB_C": connector("USB_C", ["VBUS", "CC1", "CC2", "D+", "D-", "GND", "SHIELD"]),
    "CONN2": connector("CONN2", ["1", "2"]),
    "CONN3": connector("CONN3", ["1", "2", "3"]),
    "CONN6": connector("CONN6", [str(i) for i in range(1, 7)]),
    "CONN8": connector("CONN8", [str(i) for i in range(1, 9)]),
}
PREFIX = {"R": "R", "C": "C", "L": "L", "D": "D", "FUSE": "F", "NMOS": "Q", "USB_C": "J", "CONN2": "J", "CONN3": "J",
          "CONN6": "J", "CONN8": "J"}

# ---------------------------------------------------------------- real parts
# Ultra Librarian downloads (Eagle6 Library format) in hardware/libraries/,
# read by eagle_scr.py: their symbol, footprint and pin-to-pad mapping replace
# the drawn symbol of the same name. "pins" maps this file's pin names to the
# library's where they differ (several library pins for one name = one net).
import sys
sys.path.insert(0, HERE)
import eagle_scr

LIBDIR = os.path.join(REPO, "hardware", "libraries")
LIBPARTS = {
    "BQ25798": {"dir": "ul_BQ25798RQMR",
                "pins": {"VBUS": ["VBUS", "VBUS_2"], "BAT": ["BAT", "BAT_2"], "CE": ["CE_N"], "QON": ["QON_N"],
                         "INT": ["INT_N"]}},
    "LM74700": {"dir": "ul_LM74700QDBVRQ1"},
    # Same package and pins for every TPS25982 variant; pad 25 (EPAD_2) is the
    # IN thermal pad, pad 26 (EPAD) the GND one (datasheet: pad 1 IN, pad 2 GND).
    "TPS25982": {"dir": "ul_TPS259824ONRGET", "name": "TPS259827ONRGER",
                 "pins": {"IN": ["IN", "IN_2", "IN_3", "IN_4", "EPAD_2"], "GND": ["GND", "GND_2", "GND_3", "EPAD"],
                          "OUT": ["OUT"] + [f"OUT_{i}" for i in range(2, 9)]}},
    "INA228": {"dir": "ul_INA228AIDGSR"},
    "TPS62933": {"dir": "ul_XTPS62933DRLR", "name": "TPS62933DRLR"},
    "TCA9534": {"dir": "ul_TCA9534PWR", "pins": {"INT": ["INT_N"]}},
}


def load_libparts():
    import glob
    for key, cfg in LIBPARTS.items():
        scr = glob.glob(os.path.join(LIBDIR, cfg["dir"], "Eagle", "*.scr"))
        if not scr:
            print(f"note: {cfg['dir']} not found; {key} stays a drawn symbol")
            continue
        lib = eagle_scr.Library(scr[0])
        dsname, ds = next(iter(lib.devicesets.items()))
        # TI's own land pattern ("-MFG") if there is one, else the first (nominal) one
        dev = next((d for d in ds["devices"] if d["package"].endswith("-MFG")), ds["devices"][0])
        gate, symname, _, _ = ds["gates"][0]
        sym = lib.symbols[symname]
        pins = [(p["name"], p["x"], p["y"], p["rot"]) for p in sym["pins"]]
        S = Sym(key, pins, None, None)
        S.lib = {"lib": lib, "symbol": symname, "gate": gate, "package": dev["package"],
                 "connects": dev["connects"], "prefix": ds["prefix"], "directions": {p["name"]: p["direction"] for p in sym["pins"]}}
        cfg["loaded"] = True
        SYMBOLS[key] = S


load_libparts()


def lib_pins(sym, nets):
    """This file's pin names -> the library part's (one net on several pins)."""
    cfg = LIBPARTS.get(sym)
    if not cfg or not cfg.get("loaded"):
        return nets
    m = cfg.get("pins", {})
    return {lp: net for p, net in nets.items() for lp in m.get(p, [p])}


def gate_of(sym):
    return getattr(SYMBOLS[sym], "lib", {}).get("gate", "G$1")


# ---------------------------------------------------------------- the design
# Each part: (name, symbol, value, {pin: net}). Unlisted pins are not connected.
SHEETS = []


def sheet(title, notes, board="power"):
    SHEETS.append({"title": title, "notes": notes, "parts": [], "board": board})


def part(name, sym, value, nets):
    nets = lib_pins(sym, nets)
    for p in nets:
        SYMBOLS[sym].pin(p)  # check the pin exists
    SHEETS[-1]["parts"].append((name, sym, value, nets))


def r(name, value, a, b):
    part(name, "R", value, {"1": a, "2": b})


def c(name, value, a, b="GND"):
    part(name, "C", value, {"1": a, "2": b})


# The cables between boards. Both ends use these, so the pinouts can't drift.
# CHARGER_IN / CHARGER_BAT are XT30s (pin 1 +); I2C_PORT is a JST-GH 6 that
# the charger board and the keypad both use (same cable).
CHARGER_IN = {"1": "DOCK_VIN", "2": "GND"}
CHARGER_BAT = {"1": "PACK_F", "2": "GND"}
I2C_PORT = {"1": "GND", "2": "+3V3", "3": "PWR_SDA", "4": "PWR_SCL", "5": "PWR_INT", "6": "+5V"}

sheet("1 Input: dock (LEMO)", [
    "The LEMO is the only power input: the dock, or a separate USB-C PD to LEMO charging dongle at home.",
    "Unused refs: J1, U1, D1, R1, C1, Q1-Q4, R34 (an earlier USB-C input), J4 and everything on U2 (the charger board).",
    "Dock: the LEMO 1K.308 sits in the enclosure wall and is wired to this board: J2 (XT30) takes its battery",
    "pins (LEMO 1-3 +, 4-6 ground), J7 (JST-PH 3) its data (LEMO 7, UART to the base), dock detect (8) and a ground.",
    "TVS: SMBJ18A on the dock input (16.8 V max), clamping under the 30 V FETs.",
    "U9/Q9: ideal diode right after the LEMO, so no current ever leaves the shark through it (the pack, the charger,",
    "or plan B's USB-C modules feeding back into the dock). U3/Q5 on sheet 2 is the dock's separate OR diode into",
    "the lights: without it, the pack would feed the charger's input through LIGHT_BUS.",
    "J8: a panel LED (with leads, no resistor) lit whenever the dock supplies power, firmware or not (~1-2 mA).",
])
part("J2", "CONN2", "XT30PW-M (dock power, from the LEMO)", {"1": "DOCK_RAW", "2": "GND"})
part("J7", "CONN3", "S3B-PH-K-S (dock data, from the LEMO)", {"1": "DOCK_DATA_RAW", "2": "DOCK_DET_RAW", "3": "GND"})
part("D2", "D", "SMBJ18A", {"A": "GND", "C": "DOCK_RAW"})
r("R3", "1k", "DOCK_DATA_RAW", "DOCK_UART")
part("D3", "D", "TPD1E10B06DPYR", {"A": "GND", "C": "DOCK_DATA_RAW"})
r("R4", "1k", "DOCK_DET_RAW", "DOCK_DET")
part("D4", "D", "TPD1E10B06DPYR", {"A": "GND", "C": "DOCK_DET_RAW"})
r("R5", "100k", "DOCK_DET", "GND")
part("U9", "LM74700", "LM74700-Q1 (LEMO)", {"ANODE": "DOCK_RAW", "CATHODE": "DOCK_VIN", "GATE": "GATE_LEMO",
                                            "VCAP": "VCAP_LEMO", "EN": "EN_LEMO", "GND": "GND"})
c("C48", "0.1uF", "VCAP_LEMO", "DOCK_RAW")
r("R35", "1M", "DOCK_RAW", "EN_LEMO")
part("Q9", "NMOS", "30V N-FET >= 15 A", {"G": "GATE_LEMO", "S": "DOCK_RAW", "D": "DOCK_VIN"})
r("R36", "7.5k", "DOCK_VIN", "DOCK_LED")
part("J8", "CONN2", "S2B-PH-K-S (dock LED, panel)", {"1": "DOCK_LED", "2": "GND"})

sheet("2 Lighting path: ideal diodes, eFuse, INA228", [
    "Two LM74700 ideal diodes OR the dock and the internal pack onto LIGHT_BUS. The dock's is always on.",
    "LIGHT_BUS has only small ceramics, so plugging a live dock into a switched-off shark charges ~30 uF through the",
    "LEMO, not the bulk caps: C32/C33 sit behind the eFuse (EFUSE_OUT), which ramps them up (dVdt), and hold up the",
    "converter's input while the ideal diodes switch between dock and pack.",
    "The pack's can be switched off (PACK_OFF) while the dock runs the lights; its FET's body diode still holds the bus",
    "if the dock is pulled, until the base turns it back on. Defaults to on.",
    "Pack: off-the-shelf 4S BMS on the cells, the main power switch in its lead, XT30 to J3; F1 protects lights and charging.",
    "eFuse TPS259827ONRGE: no overvoltage lockout (the pack and dock reach 16.8 V), circuit breaker.",
    "ILIM 130R = about 11.3 A (datasheet: 300R = 5 A, 182R = 8.1 A, 100R = 14.7 A). UVLO 86.6k/10k: on at 11.6 V, off at 10.6 V.",
    "LIGHT_OFF from the expander turns the lights off; defaults to on, so the lights work without the base.",
    "ITIMER, RETRY_DLY and dVdt values are starting points: check against the datasheet for the converter's input capacitance.",
    "INA228 on the output: 2 mOhm shunt, address 0x40 (A0 = A1 = GND); its ALERT shares PWR_INT.",
])
part("J3", "CONN2", "XT30 (pack, from 4S BMS)", {"1": "PACK_RAW", "2": "GND"})
part("F1", "FUSE", "15A", {"1": "PACK_RAW", "2": "PACK_F"})
part("U3", "LM74700", "LM74700-Q1 (dock)", {"ANODE": "DOCK_VIN", "CATHODE": "LIGHT_BUS", "GATE": "GATE_DOCK",
                                            "VCAP": "VCAP_DOCK", "EN": "EN_DOCK", "GND": "GND"})
c("C30", "0.1uF", "VCAP_DOCK", "DOCK_VIN")
r("R20", "1M", "DOCK_VIN", "EN_DOCK")
part("Q5", "NMOS", "30V N-FET >= 15 A", {"G": "GATE_DOCK", "S": "DOCK_VIN", "D": "LIGHT_BUS"})
part("U4", "LM74700", "LM74700-Q1 (pack)", {"ANODE": "PACK_F", "CATHODE": "LIGHT_BUS", "GATE": "GATE_PACK",
                                            "VCAP": "VCAP_PACK", "EN": "EN_PACK", "GND": "GND"})
c("C31", "0.1uF", "VCAP_PACK", "PACK_F")
r("R21", "1M", "PACK_F", "EN_PACK")
part("Q6", "NMOS", "30V N-FET >= 15 A", {"G": "GATE_PACK", "S": "PACK_F", "D": "LIGHT_BUS"})
part("Q7", "NMOS", "2N7002", {"G": "PACK_OFF", "D": "EN_PACK", "S": "GND"})
r("R22", "100k", "PACK_OFF", "GND")
c("C32", "220uF 25V polymer", "EFUSE_OUT"); c("C33", "220uF 25V polymer", "EFUSE_OUT"); c("C34", "10uF 25V", "LIGHT_BUS")
part("U5", "TPS25982", "TPS259827ONRGET", {"IN": "LIGHT_BUS", "OUT": "EFUSE_OUT", "GND": "GND", "EN/UVLO": "EFUSE_EN",
                                          "ITIMER": "EFUSE_ITIMER", "ILIM": "EFUSE_ILIM", "IMON": "EFUSE_IMON",
                                          "RETRY_DLY": "EFUSE_RETRY", "NRETRY": "GND", "LDSTRT": "GND",
                                          "PG": "EFUSE_PG", "DVDT": "EFUSE_DVDT"})
r("R23", "86.6k 1%", "LIGHT_BUS", "EFUSE_EN"); r("R24", "10k 1%", "EFUSE_EN", "GND")
part("Q8", "NMOS", "2N7002", {"G": "LIGHT_OFF", "D": "EFUSE_EN", "S": "GND"})
r("R25", "100k", "LIGHT_OFF", "GND")
r("R26", "130 1%", "EFUSE_ILIM", "GND")
c("C35", "10nF (TBD)", "EFUSE_ITIMER"); c("C36", "100nF (TBD)", "EFUSE_RETRY"); c("C37", "22nF (TBD)", "EFUSE_DVDT")
r("R27", "TBD (IMON)", "EFUSE_IMON", "GND")
r("R28", "10k", "EFUSE_PG", "+3V3")
r("R29", "2 mOhm 2512 3W", "EFUSE_OUT", "LIGHT_OUT")
part("U6", "INA228", "INA226AIDGSR", {"IN+": "EFUSE_OUT", "IN-": "LIGHT_OUT", "VBUS": "LIGHT_OUT", "VS": "+3V3",
                                "GND": "GND", "SDA": "PWR_SDA", "SCL": "PWR_SCL", "ALERT": "PWR_INT",
                                "A0": "GND", "A1": "GND"})
c("C38", "0.1uF", "+3V3")
part("J5", "CONN2", "XT30 (to the 5 V lighting converter)", {"1": "LIGHT_OUT", "2": "GND"})

sheet("3 Logic: 5 V for the base, I/O expander, ports", [
    "TPS62933 from LIGHT_BUS (dock or pack, 11-16.8 V; not the charger, so the base runs without a charger board):",
    "5 V, 3 A max, 500 kHz (RT open), FB 52.3k/10k = 4.98 V. L2 6.8 uH is a starting point (datasheet's inductor table).",
    "TCA9534 at 0x20 (A0-A2 = GND): P0 LIGHT_OFF, P1 PACK_OFF (outputs); P3 EFUSE_PG (input); P2, P4-P7 spare.",
    "Power I2C (BQ25798 0x6B, INA228 0x40, TCA9534 0x20, keypad 0x21-0x27) on its own bus from the Waveshare board;",
    "pull-ups here. J6 to the Waveshare ESP32-S3-Touch-LCD-3.49 V2: 1 +5V out, 2 GND, 3 +3V3 in (pull-ups and logic),",
    "4 SDA, 5 SCL, 6 INT (open-drain, shared by everything on the bus), 7 dock UART, 8 dock detect.",
    "Charger port: J10 (XT30, DOCK_VIN out to the charger), J11 (XT30, the charger's battery output into PACK_F),",
    "J12 (JST-GH 6: 1 GND, 2 +3V3, 3 SDA, 4 SCL, 5 INT, 6 +5V). Plan B: last year's 60 W + 140 W USB-C modules on J10/J11.",
    "Keypad port: J9, the same JST-GH 6 pinout (the keypad's expander shares INT). Leave the Waveshare battery header empty.",
])
part("U7", "TPS62933", "TPS62933DRLR", {"VIN": "LIGHT_BUS", "GND": "GND", "SW": "BUCK_SW", "BST": "BUCK_BST",
                                        "SS": "BUCK_SS", "FB": "BUCK_FB"})
c("C40", "10uF 35V", "LIGHT_BUS"); c("C41", "10uF 35V", "LIGHT_BUS"); c("C42", "0.1uF 35V", "LIGHT_BUS")
c("C43", "100nF", "BUCK_BST", "BUCK_SW")
part("L2", "L", "6.8uH Isat>=4A", {"1": "BUCK_SW", "2": "+5V"})
c("C44", "22uF 10V", "+5V"); c("C45", "22uF 10V", "+5V")
r("R30", "52.3k 1%", "+5V", "BUCK_FB"); r("R31", "10k 1%", "BUCK_FB", "GND")
c("C46", "10nF", "BUCK_SS")
part("U8", "TCA9534", "TCA9534PWR", {"VCC": "+3V3", "GND": "GND", "SDA": "PWR_SDA", "SCL": "PWR_SCL",
                                     "INT": "PWR_INT", "A0": "GND", "A1": "GND", "A2": "GND",
                                     "P0": "LIGHT_OFF", "P1": "PACK_OFF", "P3": "EFUSE_PG"})
c("C47", "0.1uF", "+3V3")
r("R32", "4.7k", "PWR_SDA", "+3V3"); r("R33", "4.7k", "PWR_SCL", "+3V3")
r("R16", "10k", "PWR_INT", "+3V3")
part("J6", "CONN8", "To the base board (JST-GH 8)", {"1": "+5V", "2": "GND", "3": "+3V3", "4": "PWR_SDA",
                                                      "5": "PWR_SCL", "6": "PWR_INT", "7": "DOCK_UART",
                                                      "8": "DOCK_DET"})
part("J10", "CONN2", "XT30 (charger port: input)", CHARGER_IN)
part("J11", "CONN2", "XT30 (charger port: battery)", CHARGER_BAT)
part("J12", "CONN6", "JST-GH 6 (charger port: I2C)", I2C_PORT)
part("J9", "CONN6", "JST-GH 6 (keypad port: I2C)", I2C_PORT)

sheet("1 Charger: BQ25798, internal pack", [
    "A separate board on the power board's charger port: J21 (XT30) DOCK_VIN in, J22 (XT30) to the pack (PACK_F,",
    "after the pack fuse on the power board), J23 (JST-GH 6) I2C, INT and supplies. Pull-ups are on the power board.",
    "BQ25798RQM: 750 kHz, 4S defaults (PROG 27.0k), so L1 = 2.2 uH (saturation >= 7 A). Datasheet SLUSDV2C.",
    "One input straight to VBUS with no ACFET-RBFETs (datasheet 7.3.5.2, Figure 8-1): VAC1 and VAC2 tied to VBUS,",
    "ACDRV1 and ACDRV2 to GND. Compare with Figure 8-1 before layout, including its optional input snubber",
    "(recommended when hot plugging adapters above 15 V). Never enable OTG: it would drive VBUS.",
    "SYS powers nothing (the base's 5 V comes from the power board's LIGHT_BUS); it keeps its capacitors.",
    "ILIM_HIZ divider 10k/10.7k from REGN: 2.59 V = a 2.0 A input clamp at power-up (VILIM = 1 V + 0.8 Ohm x ILIM),",
    "so lights (~7 A) plus charging stay under a VB99 D-Tap's 10 A with no firmware. The base can lift it by clearing",
    "EN_EXTILIM and writing IINDPM (datasheet 7.3.4.3).",
    "TS: 5.23k / 30.1k divider from REGN with a 103AT 10k NTC on the cells (J4; 0 to 60 C window).",
    "No ship FET (the pack's own BMS protects it): SDRV gets 1 nF to GND. QON left to its internal pull-up (test point).",
    "D+/D- not connected (no USB input); disable AUTO_INDET in firmware.",
], board="charger")
part("U2", "BQ25798", "BQ25798RQMR", {
    "VAC1": "DOCK_VIN", "VAC2": "DOCK_VIN", "ACDRV1": "GND", "ACDRV2": "GND", "VBUS": "DOCK_VIN",
    "PMID": "PMID", "ILIM_HIZ": "CHG_ILIM", "PROG": "CHG_PROG", "CE": "CHG_CE", "QON": "CHG_QON", "TS": "CHG_TS",
    "SDA": "PWR_SDA", "SCL": "PWR_SCL", "SW1": "SW1", "BTST1": "BTST1", "SW2": "SW2", "BTST2": "BTST2",
    "SYS": "SYS", "BAT": "PACK_F", "BATP": "BATP", "SDRV": "SDRV", "REGN": "REGN", "STAT": "CHG_STAT",
    "INT": "PWR_INT", "GND": "GND"})
part("L1", "L", "2.2uH Isat>=7A", {"1": "SW1", "2": "SW2"})
c("C10", "10uF 25V", "DOCK_VIN"); c("C11", "10uF 25V", "DOCK_VIN"); c("C12", "0.1uF 25V", "DOCK_VIN")
c("C13", "10uF 25V", "PMID"); c("C14", "10uF 25V", "PMID"); c("C15", "10uF 25V", "PMID"); c("C16", "0.1uF 25V", "PMID")
for i in range(5):
    c(f"C{17 + i}", "10uF 25V", "SYS")
c("C22", "0.1uF 25V", "SYS")
c("C23", "10uF 25V", "PACK_F"); c("C24", "10uF 25V", "PACK_F")
c("C25", "4.7uF 10V", "REGN")
c("C26", "47nF 25V", "BTST1", "SW1"); c("C27", "47nF 25V", "BTST2", "SW2")
c("C28", "1nF 50V", "SDRV")
r("R10", "27.0k 1%", "CHG_PROG", "GND")
r("R11", "100", "BATP", "PACK_F")
r("R12", "5.23k 1%", "REGN", "CHG_TS"); r("R13", "30.1k 1%", "CHG_TS", "GND")
r("R14", "10k", "CHG_CE", "GND")
r("R37", "10k 1%", "REGN", "CHG_ILIM"); r("R38", "10.7k 1%", "CHG_ILIM", "GND")
r("R15", "2.2k", "REGN", "STAT_LED")
part("LED1", "D", "LED (charging)", {"A": "STAT_LED", "C": "CHG_STAT"})
part("J4", "CONN2", "NTC 103AT on the cells", {"1": "CHG_TS", "2": "GND"})
part("J21", "CONN2", "XT30 (from the power board: input)", CHARGER_IN)
part("J22", "CONN2", "XT30 (to the power board: battery)", CHARGER_BAT)
part("J23", "CONN6", "JST-GH 6 (to the power board: I2C)", I2C_PORT)


# ---------------------------------------------------------------- footprints, part numbers
# Footprints for the drawn symbols, from KiCad's official footprint library
# (libraries/kicad/, CC-BY-SA 4.0 with an exception for designs that use it,
# see its LICENSE.md), read by kicad_mod.py; the CH224K's is drawn below from
# its datasheet. Part numbers are what JLCPCB stocked on 2026-10-24 (LCSC
# numbers for their BOM tool); passives without one are matched by value
# and size at ordering.
import kicad_mod

KICAD_DIR = os.path.join(LIBDIR, "kicad")
FOOTPRINTS = {  # variant -> KiCad footprint file
    "0603": "R_0603_1608Metric", "2512": "R_2512_6332Metric",
    "C0402": "C_0402_1005Metric", "C0603": "C_0603_1608Metric", "C0805": "C_0805_2012Metric", "C1206": "C_1206_3216Metric",
    "CP6.3x9.9": "CP_Elec_6.3x9.9", "LED0603": "LED_0603_1608Metric", "F1206": "Fuse_1206_3216Metric",
    "SMB": "D_SMB", "X1SON2": "D_0402_1005Metric", "SOT23": "SOT-23", "SO8FL": "ONSemi_SO-8FL_488AA",
    "MWSA0603S": "L_Sunlord_MWSA0603S", "USB4105": "USB_C_Receptacle_GCT_USB4105-xx-A_16P_TopMnt_Horizontal",
    "GH6": "JST_GH_SM06B-GHS-TB_1x06-1MP_P1.25mm_Horizontal", "GH8": "JST_GH_SM08B-GHS-TB_1x08-1MP_P1.25mm_Horizontal", "PH2": "JST_PH_S2B-PH-K_1x02_P2.00mm_Horizontal",
    "PH3": "JST_PH_S3B-PH-K_1x03_P2.00mm_Horizontal", "XT30": "AMASS_XT30PW-M_1x02_P2.50mm_Horizontal",
}
# symbol pin -> KiCad pad names, per (symbol, variant). KiCad diodes and LEDs: pad 1 = cathode.
PADS = {
    ("D", "SMB"): {"C": ["1"], "A": ["2"]}, ("D", "X1SON2"): {"C": ["1"], "A": ["2"]},
    ("D", "LED0603"): {"C": ["1"], "A": ["2"]},
    ("NMOS", "SOT23"): {"G": ["1"], "S": ["2"], "D": ["3"]},
    # KiCad's XT30PW-M marks pad 2 "+" and the square pad 1 "-"; symbol pin 1 is +.
    ("CONN2", "XT30"): {"1": ["2"], "2": ["1"]},
    ("NMOS", "SO8FL"): {"S": ["1", "2", "3"], "G": ["4"], "D": ["5"]},
    ("USB_C", "USB4105"): {"VBUS": ["A4", "B9", "A9", "B4"], "GND": ["A1", "B12", "A12", "B1"], "CC1": ["A5"],
                           "CC2": ["B5"], "D+": ["A6", "B6"], "D-": ["A7", "B7"], "SHIELD": ["SH"]},
    ("CH224K", "ESSOP10"): {"VDD": ["1"], "CFG1": ["2"], "CFG2": ["3"], "DP": ["4"], "DM": ["5"], "CC1": ["6"],
                            "CC2": ["7"], "VBUS": ["8"], "CFG3": ["9"], "PG": ["10"], "GND": ["EP"]},
}

# ref -> (variant, manufacturer part, LCSC). Anything not listed takes the
# default for its symbol below.
PART_INFO = {
    "U2": (None, "BQ25798RQMR", "C2876593"), "U3": (None, "LM74700QDBVRQ1", "C2941042"),
    "U4": (None, "LM74700QDBVRQ1", "C2941042"), "U5": (None, "TPS259827ONRGET", "C2155765"),
    "U6": (None, "INA226AIDGSR", "C49851"), "U7": (None, "TPS62933DRLR", "C3200405"),
    "U8": (None, "TCA9534PWR", "C783615"), "U9": (None, "LM74700QDBVRQ1", "C2941042"),
    "J2": ("XT30", "XT30PW-M", "C431092"),
    "J3": ("XT30", "XT30PW-M", "C431092"), "J4": ("PH2", "S2B-PH-K-S", "C173752"),
    "J5": ("XT30", "XT30PW-M", "C431092"), "J6": ("GH8", "SM08B-GHS-TB", "C265111"),
    "J7": ("PH3", "S3B-PH-K-S", "C157929"), "J8": ("PH2", "S2B-PH-K-S", "C173752"),
    # charger and keypad ports; the charger board's ends of the same cables
    "J10": ("XT30", "XT30PW-M", "C431092"), "J11": ("XT30", "XT30PW-M", "C431092"),
    "J21": ("XT30", "XT30PW-M", "C431092"), "J22": ("XT30", "XT30PW-M", "C431092"),
    "J9": ("GH6", "SM06B-GHS-TB", None), "J12": ("GH6", "SM06B-GHS-TB", None), "J23": ("GH6", "SM06B-GHS-TB", None),
    "D2": ("SMB", "SMBJ18A", "C10212"),
    "D3": ("X1SON2", "TPD1E10B06DPYR", "C48260"), "D4": ("X1SON2", "TPD1E10B06DPYR", "C48260"),
    "LED1": ("LED0603", "KT-0603R", "C2286"),
    "Q5": ("SO8FL", "NTMFS4C022NT1G", "C179623"), "Q6": ("SO8FL", "NTMFS4C022NT1G", "C179623"),
    "Q9": ("SO8FL", "NTMFS4C022NT1G", "C179623"),
    "Q7": ("SOT23", "2N7002", "C8545"), "Q8": ("SOT23", "2N7002", "C8545"),
    "L1": ("MWSA0603S", "MWSA0603S-2R2MT", "C408445"), "L2": ("MWSA0603S", "MWSA0603S-6R8MT", "C408449"),
    "F1": ("F1206", "CC12H15A-TR", "C2443310"), "R29": ("2512", "LR2512-23R002F4", "C154685"),
    # BQ25798 high-frequency decoupling: 0402 on top of the IC (datasheet 8.4.1, items 1 and 4)
    "C12": ("C0402", None, None), "C16": ("C0402", None, None), "C22": ("C0402", None, None),
    "C32": ("CP6.3x9.9", "KNM2220UF149EC0051", "C2981459"), "C33": ("CP6.3x9.9", "KNM2220UF149EC0051", "C2981459"),
}
DEFAULT_VARIANT = {"R": "0603", "C": "C0603", "L": "MWSA0603S", "D": "SMB", "FUSE": "F1206", "NMOS": "SOT23",
                   "USB_C": "USB4105", "CONN2": "PH2", "CONN3": "PH3", "CONN6": "GH6", "CONN8": "GH8", "CH224K": "ESSOP10"}


def cap_variant(value):
    """0603 unless the value needs a bigger case: 10-22 uF at 10-25 V -> 0805, 35 V -> 1206."""
    v = value.replace(" ", "")
    if v.startswith(("10uF", "22uF")):
        return "C1206" if "35V" in v else "C0805"
    return "C0603"


def assign_parts():
    for sh in SHEETS:
        for i, (name, sym, value, nets) in enumerate(sh["parts"]):
            info = PART_INFO.get(name)
            if info:
                variant, mpn, lcsc = info
            else:
                variant = cap_variant(value) if sym == "C" else DEFAULT_VARIANT.get(sym)
                mpn = lcsc = None
                if sym == "C" and value.replace(" ", "").startswith("10uF25V"):
                    mpn, lcsc = "CL21A106KAYNNNE", "C15850"
            if getattr(SYMBOLS[sym], "lib", None):
                variant = None
            sh["parts"][i] = (name, sym, value, nets)
            PART_EXTRA[name] = (variant, mpn, lcsc)


PART_EXTRA = {}
_footprint_cache = {}


def footprint(variant):
    if variant not in _footprint_cache:
        if variant == "ESSOP10":
            _footprint_cache[variant] = essop10()
        else:
            _footprint_cache[variant] = kicad_mod.Footprint(os.path.join(KICAD_DIR, FOOTPRINTS[variant] + ".kicad_mod"))
    return _footprint_cache[variant]


class essop10:
    """CH224K's ESSOP-10 (WCH datasheet, section 8): 1.0 mm pitch, 6.0 mm over
    the leads, 0.43 mm leads with 0.5 mm feet, 3.9 x 5.0 mm body, 2.3 x 3.2 mm
    exposed pad (GND). Pads: 0.6 x 1.4 mm centred 2.75 mm out (0.35 mm past the
    toe, 0.55 mm toward the body); pins 1-5 down the left, 6-10 up the right."""
    name = "ESSOP-10_3.9x5.0mm_P1.0mm_EP2.3x3.2mm"

    def __init__(self):
        self.items = []
        for i in range(5):
            y = 2.0 - i
            self.items.append(f'<smd name="{i + 1}" x="-2.75" y="{f(y)}" dx="1.4" dy="0.6" layer="1" roundness="25"/>')
            self.items.append(f'<smd name="{10 - i}" x="2.75" y="{f(y)}" dx="1.4" dy="0.6" layer="1" roundness="25"/>')
        self.items.append('<smd name="EP" x="0" y="0" dx="2.3" dy="3.2" layer="1" roundness="0"/>')
        for x1, y1, x2, y2 in ((-1.95, 2.6, 1.95, 2.6), (-1.95, -2.6, 1.95, -2.6)):
            self.items.append(f'<wire x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" width="0.12" layer="21"/>')
        self.items.append('<circle x="-3.3" y="2.75" radius="0.15" width="0.3" layer="21"/>')
        for x1, y1, x2, y2 in ((-1.95, -2.5, 1.95, -2.5), (1.95, -2.5, 1.95, 2.5), (1.95, 2.5, -1.95, 2.5),
                               (-1.95, 2.5, -1.95, -2.5)):
            self.items.append(f'<wire x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" width="0.1" layer="51"/>')
        self.items += ['<text x="0" y="3.2" size="1" layer="25" align="bottom-center">&gt;NAME</text>',
                       '<text x="0" y="-3.2" size="1" layer="27" align="top-center">&gt;VALUE</text>']
        self.pads = [str(i) for i in range(1, 11)] + ["EP"]

    def pads_for(self, names):
        return list(names)

    def xml(self, name=None):
        return [f'<package name="{self.name}">'] + self.items + ["</package>"]


def connects(sym, variant):
    """[(symbol pin, 'pad pad ...')] for a drawn symbol on a footprint."""
    fp = footprint(variant)
    m = PADS.get((sym, variant))
    out = []
    for pin, *_ in SYMBOLS[sym].pins:
        pads = fp.pads_for(m[pin] if m else [pin])
        missing = [p for p in pads if p not in fp.pads]
        assert not missing, f"{sym}/{variant}: pads {missing} not in the footprint"
        out.append((pin, " ".join(pads)))
    return out


# ---------------------------------------------------------------- write EAGLE XML
LAYERS = [(1, "Top", 4), (16, "Bottom", 1), (17, "Pads", 2), (18, "Vias", 2), (19, "Unrouted", 6),
          (20, "Dimension", 15), (21, "tPlace", 7), (22, "bPlace", 7), (25, "tNames", 7), (27, "tValues", 7),
          (29, "tStop", 7), (31, "tCream", 7), (39, "tKeepout", 4), (41, "tRestrict", 4), (43, "vRestrict", 2),
          (44, "Drills", 7), (45, "Holes", 7), (46, "Milling", 3), (47, "Measures", 7), (48, "Document", 7),
          (49, "Reference", 7), (51, "tDocu", 7), (52, "bDocu", 7), (91, "Nets", 2), (92, "Busses", 1), (93, "Pins", 2), (94, "Symbols", 4),
          (95, "Names", 7), (96, "Values", 7), (97, "Info", 7), (98, "Guide", 6)]


def f(v):
    return ("%.4f" % v).rstrip("0").rstrip(".")


def build(sheets):
    out = ['<?xml version="1.0" encoding="utf-8"?>', '<!DOCTYPE eagle SYSTEM "eagle.dtd">',
           '<eagle version="9.6.2">', '<drawing>',
           '<settings><setting alwaysvectorfont="no"/><setting verticaltext="up"/></settings>',
           '<grid distance="0.1" unitdist="inch" unit="inch" style="lines" multiple="1" display="no" '
           'altdistance="0.01" altunitdist="inch" altunit="inch"/>', '<layers>']
    for n, name, col in LAYERS:
        out.append(f'<layer number="{n}" name="{name}" color="{col}" fill="1" visible="yes" active="yes"/>')
    out += ['</layers>', '<schematic xreflabel="%F%N/%S.%C%R" xrefpart="/%S.%C%R">', '<libraries>',
            '<library name="nibbles_power">', '<description>Nibbles pole-base boards: symbol-only parts '
            '(the TI chips are Ultra Librarian parts with footprints; the rest are still symbol-only)</description>',
            '<packages>']
    used = sorted({p[1] for s in sheets for p in s["parts"]})
    for sname in used:
        L = getattr(SYMBOLS[sname], "lib", None)
        if L:
            out += L["lib"].package_xml(L["package"])
    variants = {}  # drawn symbol -> footprint variants used
    for s in sheets:
        for name, sym, value, nets in s["parts"]:
            v = PART_EXTRA.get(name, (None,))[0]
            if not getattr(SYMBOLS[sym], "lib", None):
                variants.setdefault(sym, set()).add(v)
    written = set()
    for vs in variants.values():
        for v in vs:
            if v and footprint(v).name not in written:
                written.add(footprint(v).name)
                out += footprint(v).xml()
    out += ['</packages>', '<symbols>']
    for sname in used:
        s = SYMBOLS[sname]
        if getattr(s, "lib", None):
            out += s.lib["lib"].symbol_xml(s.lib["symbol"], as_name=sname)
            continue
        out.append(f'<symbol name="{sname}">')
        out += s.draw
        for n, x, y, rot in s.pins:
            vis = "off" if sname in ("R", "C", "L", "D", "FUSE", "NMOS") else "pin"
            out.append(f'<pin name="{escape(n)}" x="{f(x)}" y="{f(y)}" visible="{vis}" length="short" '
                       f'direction="pas" rot="{rot}"/>')
        out.append('</symbol>')
    out += ['</symbols>', '<devicesets>']
    for sname in used:
        L = getattr(SYMBOLS[sname], "lib", None)
        if L:
            out += [f'<deviceset name="{sname}" prefix="{L["prefix"]}" uservalue="yes">', '<gates>',
                    f'<gate name="{L["gate"]}" symbol="{sname}" x="0" y="0"/>', '</gates>', '<devices>',
                    f'<device name="" package="{escape(L["package"])}">', '<connects>']
            out += [f'<connect gate="{g}" pin="{escape(pn)}" pad="{escape(pads)}"/>' for g, pn, pads in L["connects"]]
            out += ['</connects>', '<technologies>', '<technology name=""/>', '</technologies>',
                    '</device>', '</devices>', '</deviceset>']
            continue
        out += [f'<deviceset name="{sname}" prefix="{PREFIX.get(sname, "U")}" uservalue="yes">',
                '<gates>', f'<gate name="G$1" symbol="{sname}" x="0" y="0"/>', '</gates>', '<devices>']
        for v in sorted(variants.get(sname, {None}), key=lambda v: v or ""):
            if not v:
                out += ['<device name="">', '<technologies>', '<technology name=""/>', '</technologies>', '</device>']
                continue
            out += [f'<device name="{escape(v)}" package="{escape(footprint(v).name)}">', '<connects>']
            out += [f'<connect gate="G$1" pin="{escape(pn)}" pad="{escape(pads)}"/>' for pn, pads in connects(sname, v)]
            out += ['</connects>', '<technologies>', '<technology name=""/>', '</technologies>', '</device>']
        out += ['</devices>', '</deviceset>']
    out += ['</devicesets>', '</library>', '</libraries>', '<attributes>', '</attributes>',
            '<variantdefs>', '</variantdefs>', '<classes>',
            '<class number="0" name="default" width="0" drill="0">', '</class>', '</classes>', '<parts>']
    for s in sheets:
        for name, sym, value, nets in s["parts"]:
            v, mpn, lcsc = PART_EXTRA.get(name, (None, None, None))
            dev = "" if getattr(SYMBOLS[sym], "lib", None) else (v or "")
            attrs = [f'<attribute name="{k}" value="{escape(a)}"/>' for k, a in (("MPN", mpn), ("LCSC", lcsc)) if a]
            out.append(f'<part name="{name}" library="nibbles_power" deviceset="{sym}" device="{escape(dev)}" '
                       f'value="{escape(value)}"' + ('>' + "".join(attrs) + '</part>' if attrs else '/>'))
    out += ['</parts>', '<sheets>']
    parts_sym = {p[0]: p[1] for s in sheets for p in s["parts"]}
    for s in sheets:
        place, nets = layout(s)
        out += ['<sheet>', '<plain>']
        out.append(f'<text x="0" y="12.7" size="3.81" layer="97">{escape(s["title"])}</text>')
        for i, line in enumerate(s["notes"]):
            out.append(f'<text x="0" y="{f(5.08 - i * 3.81)}" size="2.1844" layer="97">{escape(line)}</text>')
        out += ['</plain>', '<instances>']
        for name, (x, y, rot) in place.items():
            r_ = f' rot="{rot}"' if rot != "R0" else ""
            out.append(f'<instance part="{name}" gate="{gate_of(parts_sym[name])}" x="{f(x)}" y="{f(y)}"{r_}/>')
        out += ['</instances>', '<busses>', '</busses>', '<nets>']
        for net, segs in sorted(nets.items()):
            out.append(f'<net name="{escape(net)}" class="0">')
            for sg in segs:
                out.append('<segment>')
                for part_name, pin in sg["pins"]:
                    out.append(f'<pinref part="{part_name}" gate="{gate_of(parts_sym[part_name])}" pin="{escape(pin)}"/>')
                for (x1, y1), (x2, y2) in sg["wires"]:
                    out.append(f'<wire x1="{f(x1)}" y1="{f(y1)}" x2="{f(x2)}" y2="{f(y2)}" width="0.1524" layer="91"/>')
                for x, y in sg["junctions"]:
                    out.append(f'<junction x="{f(x)}" y="{f(y)}"/>')
                for (x, y), lrot in sg["labels"]:
                    out.append(f'<label x="{f(x)}" y="{f(y)}" size="1.778" layer="95" rot="{lrot}"/>')
                out.append('</segment>')
            out.append('</net>')
        out += ['</nets>', '</sheet>']
    out += ['</sheets>', '</schematic>', '</drawing>', '</eagle>']
    return "\n".join(out) + "\n"


TWO_PIN = ("R", "C", "L", "D", "FUSE")
GROUP_OF = {}       # small part -> the IC or connector it hangs from (filled by layout)
STEP = {"R0": (-1, 0), "R180": (1, 0), "R90": (0, -1), "R270": (0, 1)}  # outward from a pin
LABEL_ROT = {"R0": "R180", "R180": "R0", "R90": "R270", "R270": "R90"}
PART_W = 12.7       # spacing of the hanging parts
GROUP_GAP = 7.62
LABEL_ROOM = 25.4   # between an IC and its first group: room for the pin labels


def turn(x, y, rot):
    """A symbol point turned by an instance rotation."""
    return {"R0": (x, y), "R90": (-y, x), "R180": (-x, -y), "R270": (y, -x)}[rot]


def turn_dir(pin_rot, rot):
    """A pin's own rotation, after the instance rotation."""
    order = ["R0", "R90", "R180", "R270"]
    return order[(order.index(pin_rot) + order.index(rot)) % 4]


def layout(s):
    """Places one sheet. Each IC's supporting two-pin parts hang below a wire
    from the IC pin they belong to (fanned out like a staircase so no wires
    cross); their other end gets a net label. Everything else (FETs,
    connectors, parts without an IC on the sheet) goes in rows below, with a
    labelled stub on every pin. Returns {part: (x, y, rot)} and
    {net: [segment]} with segment = {pins, wires, junctions, labels}."""
    parts = {p[0]: p for p in s["parts"]}
    CONNS = ("USB_C", "CONN8", "CONN6", "CONN3", "CONN2")
    ics = [p for p in s["parts"] if p[1] not in PREFIX or p[1] in CONNS]  # anchors: ICs and connectors
    ics.sort(key=lambda p: p[1] in CONNS)  # ICs claim shared nets first
    count = {}
    for name, sym, value, nets in s["parts"]:
        for net in nets.values():
            count[net] = count.get(net, 0) + 1
    ic_pins = {}  # net -> first (ic, pin) carrying it
    for name, sym, value, nets in ics:
        for pin, *_ in SYMBOLS[sym].pins:
            if pin in nets:
                ic_pins.setdefault(nets[pin], (name, pin))
                if pin == nets[pin]:  # a pin named like its net (REGN on REGN) beats one that only joins it
                    ic_pins[nets[pin]] = (name, pin)

    # Which IC pin each two-pin part hangs from: of its nets that reach an IC
    # pin (not GND), the most local one (fewest connections).
    groups = {}  # (ic, pin) -> [(part, top pin, bottom pin)]
    for name, sym, value, nets in s["parts"]:
        if sym not in TWO_PIN:
            continue
        pins = list(nets.items())
        cands = [(count[n], i) for i, (p, n) in enumerate(pins) if n != "GND" and n in ic_pins]
        if len(pins) != 2 or not cands:
            continue
        top = pins[min(cands)[1]]
        bottom = pins[1 - min(cands)[1]]
        groups.setdefault(ic_pins[top[1]], []).append((name, top[0], bottom[0]))

    place, segs = {}, {}
    def seg(net):
        sg = {"pins": [], "wires": [], "junctions": [], "labels": []}
        segs.setdefault(net, []).append(sg)
        return sg

    def stub(net, part_name, pin, a, d, length=5.08):
        b = (a[0] + d[0] * length, a[1] + d[1] * length)
        rot = {(-1, 0): "R180", (1, 0): "R0", (0, -1): "R270", (0, 1): "R90"}[d]
        sg = seg(net)
        sg["pins"].append((part_name, pin)); sg["wires"].append((a, b)); sg["labels"].append((b, rot))

    def pin_at(part_name, pin):
        name, sym, value, nets = parts[part_name]
        x, y, rot = place[part_name]
        _, px, py, prot = SYMBOLS[sym].pin(pin)
        dx, dy = turn(px, py, rot)
        return (x + dx, y + dy), STEP[turn_dir(prot, rot)]

    y_top = 5.08 - len(s["notes"]) * 3.81 - 25.4
    x_cursor = 0.0
    lowest = y_top
    for name, sym, value, nets in ics:
        sy = SYMBOLS[sym]
        left = [p for p in sy.pins if p[3] == "R0"]
        right = [p for p in sy.pins if p[3] == "R180"]
        gl = [(p, groups[(name, p[0])]) for p in left if (name, p[0]) in groups]
        gr = [(p, groups[(name, p[0])]) for p in right if (name, p[0]) in groups]
        # each group: its parts, then its net's label at the outer end of the rail
        label_w = lambda p: snap(len(nets[p[0]]) * 1.6 + 5.08)
        width = lambda p, g: len(g) * PART_W + label_w(p) + GROUP_GAP
        left_w = sum(width(p, g) for p, g in gl) + LABEL_ROOM
        right_w = sum(width(p, g) for p, g in gr) + LABEL_ROOM
        xs = [p[1] for p in sy.pins]
        ys = [p[2] for p in sy.pins]
        cx = snap(x_cursor + left_w - min(xs))
        cy = snap(y_top - max(ys))
        place[name] = (cx, cy, "R0")
        rail_y = snap(cy + min(ys) - 10.16)
        x_cursor = cx + max(xs) + right_w + 20.32
        for side, glist in ((-1, gl), (1, gr)):
            # Lowest pin's group nearest the IC, the top pin's farthest out, so wires never cross.
            edge = cx + (min(xs) if side < 0 else max(xs)) + side * LABEL_ROOM
            for (pname, px, py, prot), members in sorted(glist, key=lambda g: g[0][2]):
                n = len(members)
                near = snap(edge)                       # the group's end nearest the IC
                xs_parts = [snap(near + side * i * PART_W) for i in range(n)]
                edge = near + side * ((n - 1) * PART_W + label_w((pname,)) + GROUP_GAP)
                net = nets[pname]
                sg = seg(net)
                a = (cx + px, cy + py)
                sg["pins"].append((name, pname))
                corner = (xs_parts[0], a[1])
                sg["wires"] += [(a, corner), (corner, (xs_parts[0], rail_y))]
                for i, (part_name, top, bottom) in enumerate(members):
                    GROUP_OF[part_name] = name
                    x = xs_parts[i]
                    sym_p = parts[part_name][1]
                    # turn the part upright with its top pin at the rail
                    _, tx, ty, _ = SYMBOLS[sym_p].pin(top)
                    rot = "R270" if tx < 0 else "R90"
                    place[part_name] = (x, rail_y - 5.08, rot)
                    sg["pins"].append((part_name, top))
                    if i:
                        sg["wires"].append(((xs_parts[i - 1], rail_y), (x, rail_y)))
                    sg["junctions"].append((x, rail_y))  # part + rail on both sides (or drop + label end)
                    b, d = pin_at(part_name, bottom)
                    stub(parts[part_name][3][bottom], part_name, bottom, b, d, 2.54)
                    lowest = min(lowest, rail_y - 30)
                # the net's name at the far end of the rail
                end = (xs_parts[-1], rail_y)
                tip = (end[0] + side * 2.54, rail_y)
                sg["wires"].append((end, tip))
                sg["labels"].append((tip, "R0" if side > 0 else "R180"))
        # IC pins without a group: a labelled stub
        grouped = {p[0] for p, _ in gl + gr}
        for pname, px, py, prot in sy.pins:
            if pname in nets and pname not in grouped:
                stub(nets[pname], name, pname, (cx + px, cy + py), STEP[prot])
        lowest = min(lowest, cy + min(ys) - 20)

    # Everything else in rows below, as before.
    rest = [p for p in s["parts"] if p[0] not in place]
    rest.sort(key=lambda p: p[1] != "NMOS")
    col_w, cols = 66.04, 6
    y = lowest - 15.24
    for r0 in range(0, len(rest), cols):
        row = rest[r0:r0 + cols]
        tall = any(p[1] == "NMOS" for p in row)
        tall_conn = max([len(SYMBOLS[p[1]].pins) for p in row if p[1] in ("USB_C", "CONN8", "CONN6", "CONN3", "CONN2")] + [0])
        if tall:
            y -= 17.78
        y -= tall_conn * G / 2
        for i, (name, sym, value, nets) in enumerate(row):
            place[name] = (snap(25.4 + i * col_w), snap(y), "R0")
            for pin, net in nets.items():
                a, d = pin_at(name, pin)
                stub(net, name, pin, a, d)
        y -= (38.1 if tall else 15.24) + tall_conn * G / 2
    return place, segs


def snap(v):
    return round(v / G) * G


def board_sheets(board):
    return [s for s in SHEETS if s["board"] == board]


def board_nets(board):
    """net -> ['part.pin', ...] on one board."""
    netpins = {}
    for s in board_sheets(board):
        for name, sym, value, nets in s["parts"]:
            for pin, net in nets.items():
                netpins.setdefault(net, []).append(f"{name}.{pin}")
    return netpins


def main():
    names = [p[0] for s in SHEETS for p in s["parts"]]
    assert len(names) == len(set(names)), "duplicate part names"
    for s in SHEETS:
        for name, sym, value, nets in s["parts"]:
            L = getattr(SYMBOLS[sym], "lib", None)
            if L:
                for pn, d in L["directions"].items():
                    if pn not in nets and d in ("pwr", "sup"):
                        print(f"warning: {name}.{pn} is a power pin with no net")
    assign_parts()
    for board in BOARDS:
        out = out_path(board)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        open(out, "w").write(build(board_sheets(board)))
        netpins = board_nets(board)
        nparts = sum(len(s["parts"]) for s in board_sheets(board))
        print(f"{board}: {nparts} parts, {len(netpins)} nets, {len(board_sheets(board))} sheets -> "
              f"{os.path.relpath(out, REPO)}")
        lonely = {n: p for n, p in netpins.items() if len(p) < 2}
        if lonely:
            print("  nets with a single pin (cable pins, test points or unfinished):", lonely)


if __name__ == "__main__":
    main()
