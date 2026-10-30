#!/usr/bin/env python3
"""One parts order (LCSC) for every Nibbles board, with spares.

    python3 tools/hardware/combined_bom.py

Reads each board's BOM (the power board's JLCPCB BOM; the keypad boards'
schematics through kicad-cli), multiplies by how many of each board to
build (BUILDS), adds spares (SPARES), adds the extra parts in EXTRAS, then
asks LCSC for each part's stock, minimum order, order multiple and price.
Writes hardware/fab/:
  combined-LCSC-order.csv     upload to lcsc.com's BOM tool
  combined-BOM.csv            every line: per board, how many, why, stock, price
  combined-optional-LCSC.csv  cable parts (GH housings, crimps), not in the main order
Needs network access to LCSC and KiCad (kicad-cli) for the keypad boards.
"""
import csv
import json
import math
import os
import subprocess
import sys
import tempfile
import time
import urllib.request

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = os.path.join(ROOT, "hardware", "fab")
KICAD_CLI = "/Applications/KiCad/KiCad.app/Contents/MacOS/kicad-cli"

# How many of each board to build parts for: all 5 of JLCPCB's minimum PCB
# order, so every board can be assembled and there are spare built boards.
BUILDS = {
    "power_charger": 5,   # the power + charger board (fab files tagged JLCPCB-BaseController-v1.0); 1 needed
    "key_column": 5,      # 8 keys each; 2 needed (16 keys)
    "key_upper": 5,       # 3 keys; 1 needed
    "key_hub": 5,         # 1 needed
}

# Spares on top of BUILDS x per board, by part kind: (fraction, at least).
# LCSC's minimum order (often 50-100 for small passives) usually adds more.
SPARES = {
    "socket": (0.50, 10),   # the hotswap sockets: the design may change, boards may multiply
    "led": (0.50, 10),      # SK6812MINI-E: reverse mount, easy to cook with hot air
    "ic": (0.30, 2),
    "connector": (0.50, 2),
    "passive": (0.50, 10),
    "power": (0.50, 2),     # FETs, inductors, fuses, diodes, TVS
}

# Parts that aren't on a schematic. (LCSC, quantity, comment, kind)
EXTRAS = [
    # Side-entry (horizontal) JST-GH, as options for the enclosure. NOT the
    # vertical footprint: turned 180 degrees their pads land within 0.1 mm of
    # the vertical land pattern, but the pin order comes out mirrored, so
    # they need a reversed cable on that end (or the board changed to the
    # horizontal footprint).
    ("C265111", 10, "JST-GH 8 side entry SM08B-GHS-TB (option for the vertical GH8; see the notes)", "connector"),
    ("C133065", 5, "JST-GH 6 side entry SM06B-GHS-TB (option for J9/J11; see the notes)", "connector"),
    ("C189893", 10, "JST-GH 2 side entry SM02B-GHS-TB (option for the NTC ports; see the notes)", "connector"),
]
# Bench-test values for the power board's eFuse timing caps (as in its own LCSC order).
TRIALS = {"C1589": "trial: ITIMER 4.7 ms", "C21122": "trial: dVdt 80 ms ramp",
          "C1622": "trial: ITIMER 22 ms, RETRY_DLY 2.2 s", "C14663": "trial: dVdt 365 ms, RETRY_DLY 4.7 s"}
TRIAL_QTY = 10

# Used instead when LCSC can't supply the whole quantity of the original
# (checked 2026-10-30: these three went to 0 stock that day). Same footprint.
SUBSTITUTES = {
    "C15849": ("C77386", "Murata GRM188R61H105KAALD: same 1uF 50V X5R 0603"),
    "C1622": ("C107093", "Yageo CC0603KRX7R9BB473: same 47nF 50V X7R 0603"),
    "C89632": ("C13585", "Samsung CL31A106KBHNNNE: 10uF 50V 1206, X5R instead of X7R (rated to 85 C, not 125 C)"),
}

# Crimp housings and contacts for making GH cables (needs a fine crimper).
OPTIONAL = [
    ("C485357", 20, "GHR-08V-S housing (GH8 cables: keypads, hub, the 3.49)"),
    ("C190122", 10, "GHR-06V-S housing (GH6: power board keypad port)"),
    ("C189897", 300, "SSHL-002T-P0.2 crimp contact, 26-30 AWG (every GH housing)"),
]


def kind(ref, lcsc, comment):
    p = ref.rstrip("0123456789")
    if lcsc == "C41430893": return "socket"
    if lcsc == "C5149201": return "led"
    if p == "U": return "ic"
    if p == "J": return "connector"
    if p in ("R", "RN", "C"): return "passive"
    return "power"


def refs_of(s):
    return [r.strip() for r in s.split(",") if r.strip()]


def power_board():
    """Per-board lines of the power + charger board: {lcsc: [count, comment, refs]}."""
    path = os.path.join(ROOT, "hardware", "power_charger_board", "fab", "nibbles_power_charger-BOM-JLCPCB.csv")
    lines = {}
    for r in csv.DictReader(open(path)):
        refs = refs_of(r["Designator"])
        n = len(refs) * (2 if r["LCSC Part #"] == "C2932674" else 1)   # J6 is two 1x11 headers
        lines[r["LCSC Part #"]] = [n, r["Comment"], refs]
    return lines


def keypad_board(name):
    sch = os.path.join(ROOT, "hardware", "keypad", name, name + ".kicad_sch")
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "bom.csv")
        subprocess.run([KICAD_CLI, "sch", "export", "bom", "--fields", "Reference,Value,LCSC,${QUANTITY}",
                        "--labels", "Ref,Value,LCSC,Qty", "--group-by", "LCSC", "--ref-range-delimiter", "",
                        "-o", out, sch], check=True, capture_output=True)
        lines = {}
        for r in csv.DictReader(open(out)):
            if not r["LCSC"]: continue   # holes, jumpers
            v = {"C133062": "JST-GH 8 vertical",   # each board names its own; one name for the line
                 "C41430893": "Kailh CPG151101S11 hotswap socket (for the Box Navy switches)"}.get(r["LCSC"], r["Value"])
            lines[r["LCSC"]] = [int(r["Qty"]), v, refs_of(r["Ref"])]
        return lines


def lcsc(code, tries=3):
    for i in range(tries):
        try:
            req = urllib.request.Request(f"https://wmsc.lcsc.com/ftps/wm/product/detail?productCode={code}",
                                         headers={"User-Agent": "Mozilla/5.0"})
            r = json.load(urllib.request.urlopen(req, timeout=20)).get("result") or {}
            return r
        except Exception:
            time.sleep(1 + i)
    return {}


def unit_price(info, qty):
    p = None
    for step in info.get("productPriceList") or []:
        if qty >= step["ladder"]: p = step["usdPrice"]
    return p if p is not None else ((info.get("productPriceList") or [{}])[0].get("usdPrice"))


def order_qty(info, want):
    lo = info.get("minBuyNumber") or 1
    step = info.get("split") or 1
    q = max(want, lo)
    return int(math.ceil(q / step) * step)


def main():
    boards = {"power_charger": power_board()}
    for b in ("key_column", "key_upper", "key_hub"):
        boards[b] = keypad_board(b)

    parts = {}   # lcsc -> {"need", "per": {board: n}, "comments", "refs", "kind"}
    for b, lines in boards.items():
        for code, (n, comment, refs) in lines.items():
            p = parts.setdefault(code, {"need": 0, "per": {}, "comments": [], "refs": [], "kind": kind(refs[0], code, comment)})
            p["need"] += n * BUILDS[b]
            p["per"][b] = n
            if comment not in p["comments"]: p["comments"].append(comment)
            p["refs"].append(f"{b}: {','.join(refs)}")
    rows = []
    for code, p in parts.items():
        frac, least = SPARES[p["kind"]]
        spare = max(least, math.ceil(p["need"] * frac))
        notes = []
        if code in TRIALS:
            spare += TRIAL_QTY
            notes.append(f"+{TRIAL_QTY} {TRIALS[code]}")
        rows.append([code, p, spare, notes])
    for code, n, comment, k in EXTRAS:
        rows.append([code, {"need": 0, "per": {}, "comments": [comment], "refs": [], "kind": k}, n, ["not on a board yet"]])

    print(f"Looking up {len(rows) + len(OPTIONAL)} parts at LCSC...", file=sys.stderr)
    os.makedirs(OUT, exist_ok=True)
    total = 0.0
    short = []
    detail = []
    order = []
    for code, p, spare, notes in sorted(rows, key=lambda r: (r[1]["kind"], r[0])):
        info = lcsc(code)
        want = p["need"] + spare
        q = order_qty(info, want)
        if code in SUBSTITUTES and (info.get("stockNumber") or 0) < q:
            sub, why = SUBSTITUTES[code]
            notes.append(f"{sub} instead of {code} ({info.get('productModel', '')}: LCSC has {info.get('stockNumber')}): {why}")
            code, info = sub, lcsc(sub)
            q = order_qty(info, want)
        price = unit_price(info, q)
        stock = info.get("stockNumber")
        line = q * price if price is not None else 0.0
        total += line
        if not info: notes.append("NOT FOUND AT LCSC")
        elif stock is not None and stock < q: notes.append(f"SHORT: LCSC has {stock}"); short.append(code)
        if q > want: notes.append(f"LCSC minimum/multiple {info.get('minBuyNumber')}/{info.get('split')}")
        per = "; ".join(f"{b} {n} x {BUILDS[b]}" for b, n in p["per"].items())
        detail.append([code, info.get("productModel", ""), info.get("brandNameEn", ""), " / ".join(p["comments"]), p["kind"],
                       per, p["need"], spare, q, stock if stock is not None else "", f"{price:.4f}" if price is not None else "",
                       f"{line:.2f}", " | ".join(p["refs"]), "; ".join(notes)])
        order.append([code, q, " / ".join(p["comments"])[:120], ""])
        time.sleep(0.2)
    with open(os.path.join(OUT, "combined-BOM.csv"), "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["LCSC", "MPN", "Maker", "Comment", "Kind", "Per board x builds", "Needed", "Spares", "Order qty",
                    "LCSC stock", "Unit USD", "Line USD", "Designators", "Notes"])
        w.writerows(detail)
        w.writerow([]); w.writerow(["", "", "", f"Builds: {BUILDS}", "", "", "", "", "", "", "Total USD", f"{total:.2f}"])
    with open(os.path.join(OUT, "combined-LCSC-order.csv"), "w", newline="") as f:
        w = csv.writer(f); w.writerow(["LCSC Part Number", "Quantity", "Comment", "Designator"]); w.writerows(order)
    opt_total = 0.0
    with open(os.path.join(OUT, "combined-optional-LCSC.csv"), "w", newline="") as f:
        w = csv.writer(f); w.writerow(["LCSC Part Number", "Quantity", "Comment", "LCSC stock", "Line USD"])
        for code, n, comment in OPTIONAL:
            info = lcsc(code)
            q = order_qty(info, n)
            price = unit_price(info, q) or 0
            opt_total += q * price
            w.writerow([code, q, comment, info.get("stockNumber", ""), f"{q * price:.2f}"])
    print(f"{len(order)} lines, about ${total:.2f} (optional cable parts ${opt_total:.2f}); short at LCSC: {short or 'none'}")


if __name__ == "__main__":
    main()
