#!/usr/bin/env python3
"""Writes the pole-base boards as KiCad projects, one per board: schematic
(sheets under a root sheet), symbol and footprint libraries, and a board with
every footprint placed and every pad on its net.

    KPY=/Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
    $KPY tools/hardware/make_power_board_kicad.py
        writes hardware/power_board/kicad/ and hardware/charger_board/kicad/
    $KPY tools/hardware/make_power_board_kicad.py --project-only
        only the design rules and net classes in each .kicad_pro

Run it with KiCad's own Python: the board is built with KiCad's pcbnew
module. The design itself (parts, nets, layout, part numbers) is the one in
make_power_board_sch.py, which still writes the EAGLE/Fusion version.

Footprints: KiCad's own library for everything except the six TI chips
(TI's land patterns from Ultra Librarian, converted here into
nibbles_ti.pretty, which isn't committed). nibbles.pretty holds the CH224K's
ESSOP-10, drawn here; this board doesn't use it (its only power input is the
LEMO), but the USB-C to LEMO charging dongle will. Every net uses global
labels, so net names are plain (DOCK_VIN, not /Inputs/DOCK_VIN) in both the
schematic and the board.
"""
import json
import math
import os
import sys
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import make_power_board_sch as M  # noqa: E402

REPO = M.REPO
KICAD_FP = os.environ.get("KICAD_FOOTPRINTS", "/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints")
NS = uuid.UUID("6f1c2a52-7d3e-4c1b-9a51-2f6c0d6e9b11")
# The board being written (set by use_board): its folder, file name, sheets.
BOARD = OUTDIR = NAME = None
SHEET_FILES, SHEET_NAMES, SHEETS = [], [], []
SHEET_FILE_NAMES = {"power": (["1_inputs", "2_lighting", "3_logic"], ["Inputs", "Lighting", "Logic"]),
                    "charger": (["1_charger"], ["Charger"])}


def use_board(board):
    global BOARD, OUTDIR, NAME, SHEET_FILES, SHEET_NAMES, SHEETS
    folder, NAME, _ = M.BOARDS[board]
    BOARD = board
    OUTDIR = os.path.join(REPO, "hardware", folder, "kicad")
    SHEET_FILES, SHEET_NAMES = SHEET_FILE_NAMES[board]
    SHEETS = M.board_sheets(board)


def uid(*k):
    """Stable UUIDs, so regenerating keeps the schematic-to-board links."""
    return str(uuid.uuid5(NS, "/".join(str(x) for x in k)))


def f(v):
    return ("%.4f" % v).rstrip("0").rstrip(".") if abs(v) > 1e-9 else "0"


def q(s):
    return '"' + str(s).replace("\\", "\\\\").replace('"', '\\"') + '"'


# ------------------------------------------------------------ footprints
KICAD_LIB = [("CP_Elec", "Capacitor_SMD"), ("R_", "Resistor_SMD"), ("C_", "Capacitor_SMD"), ("LED_", "LED_SMD"),
             ("Fuse_", "Fuse"), ("D_", "Diode_SMD"), ("SOT-23", "Package_TO_SOT_SMD"),
             ("ONSemi_SO-8FL", "Package_SO"), ("L_Sunlord", "Inductor_SMD"), ("USB_C", "Connector_USB"),
             ("JST_", "Connector_JST"), ("AMASS_", "Connector_AMASS")]


def footprint_of(name, sym, variant):
    """(library nickname, footprint name, library folder) for a part."""
    L = getattr(M.SYMBOLS[sym], "lib", None)
    if L:
        return "nibbles_ti", L["package"], os.path.join(OUTDIR, "nibbles_ti.pretty")
    if variant == "ESSOP10":
        return "nibbles", M.essop10.name, os.path.join(OUTDIR, "nibbles.pretty")
    fp = M.FOOTPRINTS[variant]
    lib = next(v for k, v in KICAD_LIB if fp.startswith(k))
    return lib, fp, os.path.join(KICAD_FP, lib + ".pretty")


def eagle_rot(rot):
    a = float(rot.lstrip("SMR") or 0)
    return (-a) % 360


def arc_mid(p1, p2, curve):
    """Midpoint of an EAGLE arc from p1 to p2 sweeping `curve` degrees (y up)."""
    (x1, y1), (x2, y2) = p1, p2
    L = math.hypot(x2 - x1, y2 - y1)
    a = math.radians(abs(curve))
    r = L / (2 * math.sin(a / 2))
    s = r * (1 - math.cos(a / 2))
    dx, dy = (x2 - x1) / L, (y2 - y1) / L
    right = (dy, -dx) if curve > 0 else (-dy, dx)
    if abs(curve) > 180:
        s = 2 * r - s
    return ((x1 + x2) / 2 + right[0] * s, (y1 + y2) / 2 + right[1] * s)


def footprint_text(name, pads, graphics, xs, ys, descr):
    cx0, cx1, cy0, cy1 = min(xs) - 0.25, max(xs) + 0.25, min(ys) - 0.25, max(ys) + 0.25
    crt = [f'(fp_rect (start {f(cx0)} {f(cy0)}) (end {f(cx1)} {f(cy1)}) (stroke (width 0.05) (type solid)) '
           f'(fill none) (layer "F.CrtYd"))']
    out = [f'(footprint {q(name)}', '  (version 20240108)', '  (generator "nibbles")', '  (layer "F.Cu")',
           f'  (descr {q(descr)})',
           f'  (property "Reference" "REF**" (at 0 {f(cy0 - 1)} 0) (layer "F.SilkS") '
           f'(effects (font (size 1 1) (thickness 0.15))))',
           f'  (property "Value" {q(name)} (at 0 {f(cy1 + 1)} 0) (layer "F.Fab") '
           f'(effects (font (size 1 1) (thickness 0.15))))',
           '  (attr smd)']
    out += ["  " + g for g in graphics + crt + pads] + [")"]
    return "\n".join(out) + "\n"


def footprint_from_eagle(name, data, descr):
    """A KiCad footprint from Ultra Librarian's EAGLE package (y flips)."""
    copper = [p for p in data["polygon"] if p["layer"] == 1]
    pads, graphics, xs, ys = [], [], [], []
    for s in data["smd"]:
        x, y = s["x"], -s["y"]
        rot = eagle_rot(s["rot"])
        swap = rot % 180 == 90
        w, h = (s["dy"], s["dx"]) if swap else (s["dx"], s["dy"])
        xs += [x - w / 2, x + w / 2]
        ys += [y - h / 2, y + h / 2]
        layers = '"F.Cu"' + ("" if s["nocream"] else ' "F.Paste"') + ("" if s["nostop"] else ' "F.Mask"')
        poly = next((p for p in copper if inside((s["x"], s["y"]), p["pts"])), None)
        if poly:  # L-shaped corner pad: anchor + polygon
            pts = " ".join(f"(xy {f(px - s['x'])} {f(-(py - s['y']))})" for px, py in poly["pts"])
            for px, py in poly["pts"]:
                xs.append(px)
                ys.append(-py)
            pads.append(f'(pad {q(s["name"])} smd custom (at {f(x)} {f(y)}) (size {f(s["dx"])} {f(s["dy"])}) '
                        f'(layers {layers}) (options (clearance outline) (anchor rect)) '
                        f'(primitives (gr_poly (pts {pts}) (width 0) (fill yes))))')
            continue
        rr = s["roundness"] / 200
        shape = f'roundrect (at {f(x)} {f(y)} {f(rot)}) (size {f(s["dx"])} {f(s["dy"])}) (layers {layers}) ' \
                f'(roundrect_rratio {f(rr)})' if rr else \
                f'rect (at {f(x)} {f(y)} {f(rot)}) (size {f(s["dx"])} {f(s["dy"])}) (layers {layers})'
        pads.append(f'(pad {q(s["name"])} smd {shape})')
    layer_of = {21: "F.SilkS", 51: "F.Fab"}
    for w in data["wire"]:
        lay = layer_of.get(w["layer"])
        if not lay:
            continue
        (x1, y1), (x2, y2) = w["p1"], w["p2"]
        width = max(w["width"], 0.1)
        if w["curve"]:
            mx, my = arc_mid(w["p1"], w["p2"], w["curve"])
            graphics.append(f'(fp_arc (start {f(x1)} {f(-y1)}) (mid {f(mx)} {f(-my)}) (end {f(x2)} {f(-y2)}) '
                            f'(stroke (width {f(width)}) (type solid)) (layer "{lay}"))')
        else:
            graphics.append(f'(fp_line (start {f(x1)} {f(-y1)}) (end {f(x2)} {f(-y2)}) '
                            f'(stroke (width {f(width)}) (type solid)) (layer "{lay}"))')
        if lay == "F.Fab":
            xs += [x1, x2]
            ys += [-y1, -y2]
    for p in data["polygon"]:
        lay = layer_of.get(p["layer"])
        if lay:
            pts = " ".join(f"(xy {f(px)} {f(-py)})" for px, py in p["pts"])
            graphics.append(f'(fp_poly (pts {pts}) (stroke (width 0) (type solid)) (fill solid) (layer "{lay}"))')
    return footprint_text(name, pads, graphics, xs, ys, descr)


def inside(pt, poly):
    x, y = pt
    c = False
    for (x1, y1), (x2, y2) in zip(poly, poly[1:] + poly[:1]):
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            c = not c
    return c


def essop10_text():
    """The CH224K's ESSOP-10, as in make_power_board_sch.essop10 (y down here)."""
    pads = []
    for i in range(5):
        y = -2.0 + i
        pads.append(f'(pad "{i + 1}" smd roundrect (at -2.75 {f(y)}) (size 1.4 0.6) '
                    f'(layers "F.Cu" "F.Paste" "F.Mask") (roundrect_rratio 0.25))')
        pads.append(f'(pad "{10 - i}" smd roundrect (at 2.75 {f(y)}) (size 1.4 0.6) '
                    f'(layers "F.Cu" "F.Paste" "F.Mask") (roundrect_rratio 0.25))')
    pads.append('(pad "EP" smd rect (at 0 0) (size 2.3 3.2) (layers "F.Cu" "F.Paste" "F.Mask"))')
    g = ['(fp_line (start -1.95 -2.6) (end 1.95 -2.6) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))',
         '(fp_line (start -1.95 2.6) (end 1.95 2.6) (stroke (width 0.12) (type solid)) (layer "F.SilkS"))',
         '(fp_circle (center -3.75 -2.6) (end -3.6 -2.6) (stroke (width 0.3) (type solid)) (fill solid) '
         '(layer "F.SilkS"))',
         '(fp_rect (start -1.95 -2.5) (end 1.95 2.5) (stroke (width 0.1) (type solid)) (fill none) (layer "F.Fab"))']
    return footprint_text(M.essop10.name, pads, g, [-3.45, 3.45], [-2.6, 2.6],
                          "CH224K ESSOP-10 (WCH datasheet section 8): 1.0 mm pitch, 6.0 mm over the leads, "
                          "2.3 x 3.2 mm exposed pad (GND)")


def write_footprints():
    ti = os.path.join(OUTDIR, "nibbles_ti.pretty")
    own = os.path.join(OUTDIR, "nibbles.pretty")
    os.makedirs(ti, exist_ok=True)
    os.makedirs(own, exist_ok=True)
    for key, cfg in M.LIBPARTS.items():
        L = getattr(M.SYMBOLS[key], "lib", None)
        if not L:
            continue
        pkg = L["package"]
        txt = footprint_from_eagle(pkg, L["lib"].pkgdata[pkg], f"{key}: TI land pattern from Ultra Librarian")
        open(os.path.join(ti, pkg + ".kicad_mod"), "w").write(txt)
    open(os.path.join(own, M.essop10.name + ".kicad_mod"), "w").write(essop10_text())


# ------------------------------------------------------------ symbols
LEN = {"long": 7.62, "middle": 5.08, "short": 2.54, "point": 0.0}
ANGLE = {"R0": 0, "R90": 90, "R180": 180, "R270": 270}


def pin_pads(name, sym, variant):
    """symbol pin -> [pad numbers] (KiCad pad names, unmerged) for one part."""
    S = M.SYMBOLS[sym]
    L = getattr(S, "lib", None)
    if L:
        return {pin: pads.split() for _, pin, pads in L["connects"]}
    m = M.PADS.get((sym, variant), {})
    return {p[0]: m.get(p[0], [p[0]]) for p in S.pins}


def symbol_name(sym, variant):
    """One KiCad symbol per pin-to-pad numbering."""
    if getattr(M.SYMBOLS[sym], "lib", None):
        return sym
    sigs = {tuple(sorted((k, tuple(v)) for k, v in pin_pads(None, sym, v).items())) for v in VARIANTS.get(sym, [])}
    return sym if len(sigs) <= 1 else f"{sym}_{variant}"


def symbol_body(sym, S):
    """Graphics for a symbol (lib coordinates, y up)."""
    st = '(stroke (width 0.254) (type default))'
    if sym == "R":
        return [f'(rectangle (start -2.54 1.016) (end 2.54 -1.016) {st} (fill (type none)))']
    if sym == "FUSE":
        return [f'(rectangle (start -2.54 1.016) (end 2.54 -1.016) {st} (fill (type none)))',
                f'(polyline (pts (xy -2.54 0) (xy 2.54 0)) {st} (fill (type none)))']
    if sym == "C":
        return [f'(polyline (pts (xy -2.54 0) (xy -0.508 0)) {st} (fill (type none)))',
                f'(polyline (pts (xy 0.508 0) (xy 2.54 0)) {st} (fill (type none)))',
                '(polyline (pts (xy -0.508 1.524) (xy -0.508 -1.524)) (stroke (width 0.508) (type default)) (fill (type none)))',
                '(polyline (pts (xy 0.508 1.524) (xy 0.508 -1.524)) (stroke (width 0.508) (type default)) (fill (type none)))']
    if sym == "L":
        arcs = []
        for i in range(4):
            x0 = -2.54 + i * 1.27
            arcs.append(f'(arc (start {f(x0)} 0) (mid {f(x0 + 0.635)} 0.635) (end {f(x0 + 1.27)} 0) {st} (fill (type none)))')
        return arcs
    if sym == "D":
        return [f'(polyline (pts (xy -2.54 0) (xy 2.54 0)) {st} (fill (type none)))',
                f'(polyline (pts (xy -1.27 1.27) (xy 1.27 0) (xy -1.27 -1.27) (xy -1.27 1.27)) {st} (fill (type outline)))',
                f'(polyline (pts (xy 1.27 1.27) (xy 1.27 -1.27)) {st} (fill (type none)))']
    if sym == "NMOS":
        return [f'(polyline (pts (xy -2.54 0) (xy -0.762 0)) {st} (fill (type none)))',
                f'(polyline (pts (xy -0.762 1.778) (xy -0.762 -1.778)) {st} (fill (type none)))',
                f'(polyline (pts (xy 0 2.032) (xy 0 -2.032)) {st} (fill (type none)))',
                f'(polyline (pts (xy 0 1.524) (xy 2.54 1.524) (xy 2.54 2.54)) {st} (fill (type none)))',
                f'(polyline (pts (xy 0 -1.524) (xy 2.54 -1.524) (xy 2.54 -2.54)) {st} (fill (type none)))',
                f'(polyline (pts (xy 0 0) (xy 2.54 0) (xy 2.54 -1.524)) {st} (fill (type none)))']
    # ICs and connectors: a box between the pins
    lens = pin_lengths(sym, S)
    left = [p for p in S.pins if p[3] == "R0"]
    right = [p for p in S.pins if p[3] == "R180"]
    x0 = min([p[1] + lens[p[0]] for p in left] or [-2.54])
    x1 = max([p[1] - lens[p[0]] for p in right] or [x0 + 5.08])
    ys = [p[2] for p in S.pins]
    return [f'(rectangle (start {f(x0)} {f(max(ys) + 2.54)}) (end {f(x1)} {f(min(ys) - 2.54)}) {st} '
            f'(fill (type background)))']


def pin_lengths(sym, S):
    L = getattr(S, "lib", None)
    if L:
        return {p["name"]: LEN.get(p["length"], 2.54) for p in L["lib"].symbols[L["symbol"]]["pins"]}
    return {p[0]: 2.54 for p in S.pins}


def symbol_text(kname, sym, variant, prefix, embedded):
    S = M.SYMBOLS[sym]
    pads = pin_pads(None, sym, variant)
    lens = pin_lengths(sym, S)
    small = sym in M.PREFIX and not sym.startswith(("CONN", "USB"))
    full = f"nibbles:{kname}" if embedded else kname
    eff = '(effects (font (size 1.27 1.27)))'
    hid = '(effects (font (size 1.27 1.27)) hide)'
    out = [f'(symbol {q(full)}' + (' (pin_numbers hide) (pin_names (offset 0) hide)' if small else
                                     ' (pin_names (offset 1.016))') +
           ' (exclude_from_sim no) (in_bom yes) (on_board yes)',
           f'  (property "Reference" {q(prefix)} (at 0 3.81 0) {eff})',
           f'  (property "Value" {q(kname)} (at 0 -3.81 0) {eff})',
           f'  (property "Footprint" "" (at 0 0 0) {hid})',
           f'  (property "Datasheet" "" (at 0 0 0) {hid})',
           f'  (symbol {q(kname + "_0_1")}'] + ["    " + g for g in symbol_body(sym, S)] + ['  )',
                                                                                         f'  (symbol {q(kname + "_1_1")}']
    for pname, x, y, rot in S.pins:
        for i, num in enumerate(pads.get(pname, [pname])):
            out.append(f'    (pin passive line (at {f(x)} {f(y)} {ANGLE[rot]}) (length {f(lens[pname])})'
                       + (' hide' if i else '') + f' (name {q(pname)} {eff}) (number {q(num)} {eff}))')
    out += ['  )', ')']
    return out


# ------------------------------------------------------------ schematic
VARIANTS = {}  # drawn symbol -> footprint variants in use


def collect_variants():
    for s in M.SHEETS:
        for name, sym, value, nets in s["parts"]:
            v = M.PART_EXTRA[name][0]
            if not getattr(M.SYMBOLS[sym], "lib", None):
                VARIANTS.setdefault(sym, [])
                if v not in VARIANTS[sym]:
                    VARIANTS[sym].append(v)


def label_justify(a, glob):
    if glob:
        return "right" if a in (180, 270) else "left"
    return "right bottom" if a in (180, 270) else "left bottom"


def write_sheet(idx, s, root_uuid, sheet_uuid, lib_symbols_txt):
    place, segs = M.layout(s)
    pts = []
    for net, sl in segs.items():
        for sg in sl:
            for a, b in sg["wires"]:
                pts += [a, b]
    pts += [(0, 12.7)] + [(0, 5.08 - i * 3.81) for i in range(len(s["notes"]))]
    pts += [(x, y) for x, y, _ in place.values()]
    ox = 25.4 - min(p[0] for p in pts) + 0.0
    oy = 38.1 + max(p[1] for p in pts)  # kicad y = oy - y
    ox, oy = round(ox / 2.54) * 2.54, round(oy / 2.54) * 2.54
    X = lambda x: x + ox
    Y = lambda y: oy - y
    w = max(X(p[0]) for p in pts) + 60
    h = max(Y(p[1]) for p in pts) + 40
    path = f"/{root_uuid}/{sheet_uuid}"
    out = ['(kicad_sch (version 20231120) (generator "nibbles") (generator_version "8.0")',
           f'  (uuid {q(uid("sheetfile", idx))})', f'  (paper "User" {f(w)} {f(h)})',
           f'  (title_block (title {q(s["title"])}) (company "Nibbles") '
           f'(comment 1 "Generated by tools/hardware/make_power_board_kicad.py"))',
           '  (lib_symbols'] + ["    " + l for l in lib_symbols_txt] + ['  )']
    out.append(f'  (text {q(s["title"])} (exclude_from_sim no) (at {f(X(0))} {f(Y(12.7))} 0) '
               f'(effects (font (size 3.81 3.81) bold) (justify left bottom)) (uuid {q(uid("title", idx))}))')
    for i, line in enumerate(s["notes"]):
        out.append(f'  (text {q(line)} (exclude_from_sim no) (at {f(X(0))} {f(Y(5.08 - i * 3.81))} 0) '
                   f'(effects (font (size 2 2)) (justify left bottom)) (uuid {q(uid("note", idx, i))}))')
    n = 0
    for net, sl in sorted(segs.items()):
        for sg in sl:
            for a, b in sg["wires"]:
                n += 1
                out.append(f'  (wire (pts (xy {f(X(a[0]))} {f(Y(a[1]))}) (xy {f(X(b[0]))} {f(Y(b[1]))})) '
                           f'(stroke (width 0) (type default)) (uuid {q(uid("wire", idx, net, n))}))')
            for j in sg["junctions"]:
                n += 1
                out.append(f'  (junction (at {f(X(j[0]))} {f(Y(j[1]))}) (diameter 0) (color 0 0 0 0) '
                           f'(uuid {q(uid("junction", idx, net, n))}))')
            for (x, y), lrot in sg["labels"]:
                n += 1
                a = ANGLE[lrot]
                out.append(f'  (global_label {q(net)} (shape passive) (at {f(X(x))} {f(Y(y))} {a}) '
                           f'(fields_autoplaced yes) (effects (font (size 1.27 1.27)) (justify {label_justify(a, True)})) '
                           f'(uuid {q(uid("label", idx, net, n))}) '
                           f'(property "Intersheetrefs" "${{INTERSHEET_REFS}}" (at {f(X(x))} {f(Y(y))} 0) '
                           f'(effects (font (size 1.27 1.27)) hide)))')
    for name, sym, value, nets in s["parts"]:  # pins meant to be open get a no-connect flag
        x, y, rot = place[name]
        for pname, px, py, _ in M.SYMBOLS[sym].pins:
            if pname not in nets:
                dx, dy = M.turn(px, py, rot)
                n += 1
                out.append(f'  (no_connect (at {f(X(x + dx))} {f(Y(y + dy))}) (uuid {q(uid("nc", name, pname))}))')
    for name, sym, value, nets in s["parts"]:
        x, y, rot = place[name]
        variant, mpn, lcsc = M.PART_EXTRA[name]
        kname = symbol_name(sym, variant)
        lib, fpname, _ = footprint_of(name, sym, variant)
        su = uid("symbol", name)
        a = ANGLE[rot]
        vertical = a in (90, 270)
        rx, ry = (X(x) + 2.54, Y(y) - 1.27) if vertical else (X(x) - 2.54, Y(y) - 3.81)
        vx, vy = (X(x) + 2.54, Y(y) + 1.27) if vertical else (X(x) - 2.54, Y(y) + 3.81)
        if sym not in M.PREFIX or sym.startswith(("CONN", "USB")):
            ys = [p[2] for p in M.SYMBOLS[sym].pins]
            xs = [p[1] for p in M.SYMBOLS[sym].pins]
            rx, ry = X(x + min(xs)), Y(y + max(ys) + 3.81)
            vx, vy = X(x + min(xs)), Y(y + min(ys) - 5.08)
        eff = '(effects (font (size 1.27 1.27)) (justify left))'
        hid = '(effects (font (size 1.27 1.27)) hide)'
        out += [f'  (symbol (lib_id {q("nibbles:" + kname)}) (at {f(X(x))} {f(Y(y))} {a}) (unit 1) '
                f'(exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) (uuid {q(su)})',
                f'    (property "Reference" {q(name)} (at {f(rx)} {f(ry)} 0) {eff})',
                f'    (property "Value" {q(value)} (at {f(vx)} {f(vy)} 0) {eff})',
                f'    (property "Footprint" {q(lib + ":" + fpname)} (at {f(X(x))} {f(Y(y))} 0) {hid})',
                f'    (property "Datasheet" "" (at {f(X(x))} {f(Y(y))} 0) {hid})']
        if mpn:
            out.append(f'    (property "MPN" {q(mpn)} (at {f(X(x))} {f(Y(y))} 0) {hid})')
        if lcsc:
            out.append(f'    (property "LCSC" {q(lcsc)} (at {f(X(x))} {f(Y(y))} 0) {hid})')
        for pin, pads in pin_pads(name, sym, variant).items():
            for num in pads:
                out.append(f'    (pin {q(num)} (uuid {q(uid("pin", name, num))}))')
        out += [f'    (instances (project {q(NAME)} (path {q(path)} (reference {q(name)}) (unit 1))))', '  )']
    out.append(')')
    open(os.path.join(OUTDIR, SHEET_FILES[idx] + ".kicad_sch"), "w").write("\n".join(out) + "\n")


ROOT_TEXT = {
    "power": "Pole-base power board: dock (LEMO) input, ideal-diode lighting path with eFuse and INA226, "
             "5 V for the base board, I/O expander, charger and keypad ports. Design notes are on each sheet.",
    "charger": "Charger board: BQ25798 buck-boost charger for the 4S pack, on the power board's charger port. "
               "Design notes are on the sheet.",
}


def write_root(root_uuid, sheet_uuids):
    out = ['(kicad_sch (version 20231120) (generator "nibbles") (generator_version "8.0")',
           f'  (uuid {q(root_uuid)})', '  (paper "A4")',
           f'  (title_block (title {q(M.BOARDS[BOARD][2])}) (company "Nibbles") '
           f'(comment 1 "Generated by tools/hardware/make_power_board_kicad.py; see hardware/{M.BOARDS[BOARD][0]}/README.md"))',
           '  (lib_symbols)',
           f'  (text {q(ROOT_TEXT[BOARD])} (exclude_from_sim no) (at 25.4 25.4 0) '
           '(effects (font (size 2 2)) (justify left bottom)) (uuid ' + q(uid("roottext", BOARD)) + '))']
    for i, (fn, nm, su) in enumerate(zip(SHEET_FILES, SHEET_NAMES, sheet_uuids)):
        x, y = 25.4 + (i % 2) * 101.6, 45.72 + (i // 2) * 50.8
        out += [f'  (sheet (at {f(x)} {f(y)}) (size 76.2 30.48) (fields_autoplaced yes) '
                f'(stroke (width 0.1524) (type solid)) (fill (color 0 0 0 0.0000)) (uuid {q(su)})',
                f'    (property "Sheetname" {q(nm)} (at {f(x)} {f(y - 0.7)} 0) '
                f'(effects (font (size 1.27 1.27)) (justify left bottom)))',
                f'    (property "Sheetfile" {q(fn + ".kicad_sch")} (at {f(x)} {f(y + 31.1)} 0) '
                f'(effects (font (size 1.27 1.27)) (justify left top)))',
                f'    (instances (project {q(NAME)} (path {q("/" + root_uuid)} (page {q(str(i + 2))}))))', '  )']
    out += ['  (sheet_instances (path "/" (page "1")))', ')']
    open(os.path.join(OUTDIR, NAME + ".kicad_sch"), "w").write("\n".join(out) + "\n")


def write_project():
    pro = {"meta": {"filename": NAME + ".kicad_pro", "version": 1},
           "board": {"design_settings": {"defaults": {}, "rules": {
               # JLCPCB's standard 2- and 4-layer capabilities (conservative)
               "min_clearance": 0.127, "min_track_width": 0.127, "min_via_diameter": 0.6,
               "min_through_hole_diameter": 0.3, "min_copper_edge_clearance": 0.3, "min_hole_clearance": 0.25}}},
           # 0.15 mm default clearance: TI's TPS25982 land pattern has 0.165 mm gaps to its thermal pads
           "net_settings": {"classes": [{"name": "Default", "clearance": 0.15, "track_width": 0.25,
                                         "via_diameter": 0.6, "via_drill": 0.3}]},
           "text_variables": {}}
    json.dump(pro, open(os.path.join(OUTDIR, NAME + ".kicad_pro"), "w"), indent=2)
    open(os.path.join(OUTDIR, "fp-lib-table"), "w").write(
        '(fp_lib_table (version 7)\n'
        '  (lib (name "nibbles")(type "KiCad")(uri "${KIPRJMOD}/nibbles.pretty")(options "")'
        '(descr "Nibbles: the CH224K footprint (for the USB-C to LEMO dongle; unused here)"))\n'
        '  (lib (name "nibbles_ti")(type "KiCad")(uri "${KIPRJMOD}/nibbles_ti.pretty")(options "")'
        '(descr "TI land patterns from Ultra Librarian (regenerate; not committed)"))\n)\n')
    open(os.path.join(OUTDIR, "sym-lib-table"), "w").write(
        '(sym_lib_table (version 7)\n'
        '  (lib (name "nibbles")(type "KiCad")(uri "${KIPRJMOD}/nibbles.kicad_sym")(options "")'
        '(descr "Nibbles power board symbols"))\n)\n')


# Track widths (IPC-2221, outer layers, 10 C rise; docs in each board's
# README): the power board is ordered with 2 oz copper, the charger board
# 1 oz. Ground stays Default: it's a plane and pours. Clearance stays 0.15 mm
# everywhere (the TPS25982's pads). class -> (settings, nets)
NETCLASSES = {
    "power": {
        # the lights' path and its sources: 8 A continuous, ~11 A worst case
        "Power": ({"track_width": 5.0, "via_diameter": 1.0, "via_drill": 0.5},
                  ["DOCK_RAW", "DOCK_VIN", "LIGHT_BUS", "EFUSE_OUT", "LIGHT_OUT", "PACK_RAW", "PACK_F"]),
        # 5 V buck: up to 3 A
        "Supply": ({"track_width": 1.0, "via_diameter": 0.8, "via_drill": 0.4}, ["+5V", "BUCK_SW"]),
    },
    "charger": {
        # input, charge and switching currents: up to ~6 A in the inductor when boosting
        "Power": ({"track_width": 3.0, "via_diameter": 0.8, "via_drill": 0.4},
                  ["DOCK_VIN", "PMID", "SYS", "SW1", "SW2", "PACK_F"]),
    },
}


def patch_project():
    """JLCPCB-friendly rules into the project file KiCad wrote with the board."""
    path = os.path.join(OUTDIR, NAME + ".kicad_pro")
    pro = json.load(open(path))
    rules = pro.setdefault("board", {}).setdefault("design_settings", {}).setdefault("rules", {})
    rules.update({
        # JLCPCB's standard processes manage 0.1 mm spacing; keep some margin.
        "min_clearance": 0.127, "min_track_width": 0.127, "min_via_diameter": 0.6,
        "min_through_hole_diameter": 0.3, "min_copper_edge_clearance": 0.3, "min_hole_clearance": 0.25})
    # Pre-defined sizes for the track width / via size menus (and W / Shift+W while
    # routing), so a Power net can neck down into small pins: the ideal diodes'
    # ANODE/CATHODE sense pins, pull-ups and dividers carry almost no current.
    ds = pro["board"]["design_settings"]
    ds["track_widths"] = [0.0, 0.25, 0.5, 1.0, 1.5, 3.0, 5.0]  # the first entry is the net class width
    ds["via_dimensions"] = [{"diameter": 0.0, "drill": 0.0}, {"diameter": 0.6, "drill": 0.3},
                            {"diameter": 0.8, "drill": 0.4}, {"diameter": 1.0, "drill": 0.5}]
    classes = pro.setdefault("net_settings", {}).setdefault("classes", [])
    default = next((c for c in classes if c.get("name") == "Default"), None)
    if default is None:
        default = {"name": "Default"}
        classes.append(default)
    # 0.15 mm: TI's TPS25982 land pattern has 0.165 mm gaps to its thermal pads.
    default.update({"clearance": 0.15, "track_width": 0.25, "via_diameter": 0.6, "via_drill": 0.3})
    # This board's net classes (replacing any earlier ones), so routing picks the width per net.
    ns = pro["net_settings"]
    ns["classes"] = [default] + [dict(default, name=name, priority=i, **cfg)
                                 for i, (name, (cfg, _)) in enumerate(NETCLASSES[BOARD].items())]
    ns["netclass_patterns"] = [{"netclass": name, "pattern": net}
                               for name, (_, nets) in NETCLASSES[BOARD].items() for net in nets]
    json.dump(pro, open(path, "w"), indent=2)


# ------------------------------------------------------------ board
# Rough placement, millimetres from each board's top-left corner (KiCad: y
# down; rotation counter-clockwise), on placeholder outlines. Connector
# rotations put the mating side on the board edge: XT30 0 = top, 90 = left,
# 180 = bottom, 270 = right (centre 13.6 mm in from that edge); JST-PH 180 =
# top, 270 = left (centre 6.8 mm in); JST-GH 180 = top, 0 = bottom, 90 =
# right (centre 3.2 mm in).
ORIGIN = (50.0, 50.0)  # where a board sits on KiCad's page

# Charger board: the BQ25798 cluster, relative to U2 (pins 25-29
# SYS/SW2/GND/SW1/PMID face up), after the datasheet's layout priorities
# (8.4.1) and Figure 8-21: 0402s from PMID and SYS to GND right at those
# pins with a 10 uF beside each, the rest of PMID's caps on the left and
# SYS's on the right, the inductor just above (SW1/SW2 reach it through vias
# under the IC, on In2), the VBUS 0402 and REGN cap at the left pins, BAT
# caps at the right pins, the slow parts below. Input on the left edge,
# battery on the right, I2C and the NTC on the bottom.
U2_AT = (27.0, 16.0)
CHARGER = {
    "U2": (0, 0, 0),
    "C16": (-1.0, -3.1, 0), "C22": (1.0, -3.1, 180),  # PMID, SYS 0.1 uF
    "C13": (-2.0, -5.0, 0), "C17": (2.0, -5.0, 180),  # PMID, SYS 10 uF
    "L1": (0, -10.5, 0),
    "C14": (-7.0, -5.5, 0), "C15": (-7.0, -8.0, 0),  # PMID
    "C18": (7.2, -5.5, 180), "C19": (7.2, -8.0, 180), "C20": (7.2, -10.5, 180), "C21": (7.2, -13.0, 180),  # SYS
    "C12": (-3.0, -3.1, 0), "C26": (-5.0, -1.4, 0), "C25": (-4.6, 0.4, 0),  # VBUS 0.1 uF, BTST1, REGN
    "C10": (-8.2, 1.6, 0), "C11": (-8.2, 3.9, 0),  # VBUS 10 uF
    "C27": (4.3, -3.2, 0), "C28": (8.4, -3.4, 0),  # BTST2, SDRV
    "C23": (4.6, -1.0, 180), "C24": (8.4, -1.0, 180),  # BAT
    "R11": (4.3, 1.0, 0), "R10": (4.3, 2.6, 0),  # BATP, PROG
    "R15": (-4.0, 4.2, 0), "LED1": (-4.0, 5.9, 0), "R14": (-0.6, 4.2, 0),  # STAT LED, CE
    "R12": (2.6, 4.2, 0), "R13": (2.6, 5.9, 0),  # TS divider
    "R37": (6.2, 4.2, 0), "R38": (6.2, 5.9, 0),  # ILIM_HIZ divider
}
PLACE_CHARGER = {k: (U2_AT[0] + x, U2_AT[1] + y, r) for k, (x, y, r) in CHARGER.items()}
PLACE_CHARGER.update({
    "J21": (13.6, 16.0, 90), "J22": (40.4, 16.0, 270),
    "J23": (27.0, 29.8, 0), "J4": (37.0, 26.2, 0),
})

# Power board: power runs left to right. The LEMO's power (J2) top left,
# through its ideal diode (U9/Q9) to DOCK_VIN; the charger port (J10, J11,
# J12) and the dock LED (J8) along the top edge; the dock's OR diode (U3/Q5)
# and the pack's (U4/Q6, from J3 on the left) feed LIGHT_BUS, then the
# eFuse, shunt and the lights' XT30 (J5) on the right. The 5 V buck, the
# I/O expander, the base connector (J6, right) and the keypad port (J9,
# bottom) are bottom right.
PLACE_POWER = {
    # left edge: from the LEMO and the pack
    "J2": (13.6, 8.0, 90), "D2": (21.0, 4.5, 0),
    "Q9": (22.5, 12.0, 0), "U9": (30.0, 12.0, 0), "C48": (34.5, 10.5, 0), "R35": (34.5, 13.5, 0),
    "R36": (33.0, 4.0, 0), "J8": (40.0, 6.8, 180),
    "J7": (6.8, 28.0, 270), "D3": (12.0, 26.5, 0), "R3": (12.0, 28.5, 0), "D4": (12.0, 30.5, 0),
    "R4": (12.0, 32.5, 0), "R5": (16.5, 32.5, 0),
    "J3": (13.6, 46.0, 90), "F1": (19.5, 46.0, 0),
    # charger port along the top edge
    "J10": (54.0, 13.6, 0), "J11": (70.0, 13.6, 0), "J12": (82.0, 3.2, 180),
    # dock OR diode (top) and pack OR diode (bottom) into LIGHT_BUS
    "Q5": (36.0, 22.0, 0), "U3": (43.0, 22.0, 0), "C30": (47.5, 20.5, 0), "R20": (47.5, 23.5, 0),
    "Q6": (30.0, 46.0, 0), "U4": (37.0, 46.0, 0), "C31": (37.0, 50.5, 0), "R21": (41.5, 46.0, 0),
    "Q7": (42.0, 51.0, 0), "R22": (46.0, 51.0, 0),
    "C34": (58.0, 34.0, 0),
    # eFuse, its settings below it; shunt and INA226 on to the lights' XT30
    "U5": (70.0, 30.0, 0), "R23": (68.0, 36.0, 0), "R24": (68.0, 38.0, 0), "Q8": (73.0, 37.0, 0),
    "R25": (77.5, 37.0, 0), "R26": (68.0, 40.0, 0), "C35": (68.0, 42.0, 0), "C36": (72.0, 41.0, 0),
    "C37": (72.0, 43.0, 0), "R27": (76.0, 41.0, 0), "R28": (76.0, 43.0, 0),
    "C32": (69.0, 21.5, 0), "C33": (79.0, 32.0, 0),  # bulk caps behind the eFuse
    "R29": (80.0, 23.0, 90), "U6": (80.0, 16.0, 0), "C38": (85.0, 15.0, 0),
    "J5": (86.4, 30.0, 270),
    # 5 V buck from LIGHT_BUS, I/O expander, base connector, keypad port
    "C40": (53.0, 52.0, 0), "C41": (53.0, 55.0, 0), "C42": (53.0, 58.0, 0),
    "U7": (60.0, 55.0, 0), "C43": (60.0, 51.5, 0), "L2": (68.0, 55.0, 0),
    "C44": (76.0, 53.0, 0), "C45": (76.0, 56.0, 0), "R30": (60.0, 59.0, 0), "R31": (60.0, 61.0, 0),
    "C46": (66.0, 61.0, 0),
    "U8": (72.0, 63.0, 0), "C47": (72.0, 67.5, 0), "R32": (86.0, 58.0, 0), "R33": (86.0, 60.0, 0),
    "R16": (86.0, 62.0, 0),
    "J6": (96.8, 58.0, 90), "J9": (84.0, 66.8, 0),
}

# board -> placement, outline (placeholder), copper layers, the plane's layer, outline note
LAYOUT = {
    "power": {"place": PLACE_POWER, "size": (100.0, 70.0), "layers": 2, "plane": "B.Cu",
              "note": "Left edge: LEMO (dock power J2, dock signals J7) and pack (J3). "
                      "Top edge: dock LED (J8), charger port (J10 input, J11 battery, J12 I2C). "
                      "Right edge: lights (J5), base board (J6). Bottom edge: keypad port (J9)."},
    "charger": {"place": PLACE_CHARGER, "size": (54.0, 33.0), "layers": 4, "plane": "In1.Cu",
                "note": "Left edge: input from the power board (J21). Right edge: battery (J22). "
                        "Bottom edge: I2C (J23), NTC on the cells (J4)."},
}


def check_board_unrouted():
    """Refuse to overwrite a board someone has started routing in KiCad (set
    NIBBLES_OVERWRITE_BOARD=1 to overwrite anyway). Placement-only edits
    aren't caught here; KiCad's .history/ keeps those."""
    path = os.path.join(OUTDIR, NAME + ".kicad_pcb")
    if os.path.exists(path) and not os.environ.get("NIBBLES_OVERWRITE_BOARD"):
        txt = open(path).read()
        if "(segment" in txt or "(via" in txt or "(arc" in txt:
            sys.exit(f"{os.path.relpath(path, REPO)} has tracks (routed in KiCad): not overwriting it. "
                     "Make changes in KiCad, or set NIBBLES_OVERWRITE_BOARD=1.")


def write_board(root_uuid, sheet_uuids):
    import pcbnew
    board = pcbnew.BOARD()
    nets = {}

    def net(name):
        if name not in nets:
            n = pcbnew.NETINFO_ITEM(board, name)
            board.Add(n)
            nets[name] = n
        return nets[name]

    mm = pcbnew.FromMM
    blocks = []  # per sheet: [(anchor, [members])...], rest
    for si, s in enumerate(SHEETS):
        parts = {p[0]: p for p in s["parts"]}
        anchors = [p[0] for p in s["parts"] if any(M.GROUP_OF.get(q) == p[0] for q in parts)]
        grouped = set(anchors) | {q for q in parts if M.GROUP_OF.get(q) in anchors}
        rest = [p[0] for p in s["parts"] if p[0] not in grouped]
        blocks.append((si, [(a, [q for q in parts if M.GROUP_OF.get(q) == a]) for a in anchors], rest))
    fps = {}
    for si, s in enumerate(SHEETS):
        for name, sym, value, pinnets in s["parts"]:
            variant, mpn, lcsc = M.PART_EXTRA[name]
            lib, fpname, folder = footprint_of(name, sym, variant)
            fp = pcbnew.FootprintLoad(folder, fpname)
            assert fp, f"footprint {lib}:{fpname} not found in {folder}"
            fp.SetFPIDAsString(f"{lib}:{fpname}")
            fp.SetReference(name)
            fp.SetValue(value)
            if mpn:
                fp.SetField("MPN", mpn)
            if lcsc:
                fp.SetField("LCSC", lcsc)
            for fl in fp.GetFields():  # part numbers are for the BOM, not the silkscreen
                if fl.GetName() in ("MPN", "LCSC"):
                    fl.SetVisible(False)
                    fl.SetLayer(pcbnew.F_Fab)
            fp.SetPath(pcbnew.KIID_PATH(f"/{sheet_uuids[si]}/{uid('symbol', name)}"))
            padnet = {}
            for pin, pads in pin_pads(name, sym, variant).items():
                if pin in pinnets:
                    for num in pads:
                        padnet[num] = pinnets[pin]
            for pad in fp.Pads():
                n = padnet.get(pad.GetNumber())
                if n:
                    pad.SetNet(net(n))
            board.Add(fp)
            fps[name] = fp

    # Rough placement (see LAYOUT): parts where they belong relative to each
    # other, on a placeholder outline, with a solid ground plane.
    lay = LAYOUT[BOARD]
    BOARD_W, BOARD_H = lay["size"]
    board.SetCopperLayerCount(lay["layers"])
    missing = set(fps) - set(lay["place"])
    assert not missing, f"no placement for {sorted(missing)}"
    extra = set(lay["place"]) - set(fps)
    assert not extra, f"placement for parts not on this board: {sorted(extra)}"
    for ref, (x, y, rot) in lay["place"].items():
        fp = fps[ref]
        fp.SetOrientationDegrees(rot)
        fp.SetPosition(pcbnew.VECTOR2I(mm(ORIGIN[0] + x), mm(ORIGIN[1] + y)))
    x0, y0, x1, y1 = (mm(ORIGIN[0]), mm(ORIGIN[1]), mm(ORIGIN[0] + BOARD_W), mm(ORIGIN[1] + BOARD_H))
    edge = pcbnew.PCB_SHAPE(board)
    edge.SetShape(pcbnew.SHAPE_T_RECT)
    edge.SetStart(pcbnew.VECTOR2I(x0, y0))
    edge.SetEnd(pcbnew.VECTOR2I(x1, y1))
    edge.SetLayer(pcbnew.Edge_Cuts)
    edge.SetWidth(mm(0.05))
    board.Add(edge)
    note = pcbnew.PCB_TEXT(board)
    note.SetText(f"Placeholder outline {BOARD_W:g} x {BOARD_H:g} mm: resize to the enclosure. " + lay["note"])
    note.SetLayer(pcbnew.Cmts_User)
    note.SetPosition(pcbnew.VECTOR2I(x0, y0 - mm(3)))
    note.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
    board.Add(note)
    gnd = pcbnew.ZONE(board)
    gnd.SetLayer(board.GetLayerID(lay["plane"]))
    gnd.SetNet(net("GND"))
    gnd.SetZoneName("GND plane")
    ol = gnd.Outline()
    ol.NewOutline()
    for px, py in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        ol.Append(px, py)
    board.Add(gnd)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.SetFileName(os.path.join(OUTDIR, NAME + ".kicad_pcb"))
    pcbnew.SaveBoard(os.path.join(OUTDIR, NAME + ".kicad_pcb"), board)
    return len(fps), len(nets)


# ------------------------------------------------------------ main
def main():
    M.assign_parts()
    collect_variants()
    for board in M.BOARDS:
        use_board(board)
        if "--project-only" in sys.argv:  # rules and net classes only; schematic and board untouched
            patch_project()
            print(f"rules and net classes -> {os.path.relpath(OUTDIR, REPO)}/{NAME}.kicad_pro")
        else:
            write_project_files()


def write_project_files():
    os.makedirs(OUTDIR, exist_ok=True)
    check_board_unrouted()
    for fn in os.listdir(OUTDIR):  # sheets left from an earlier split
        if fn.endswith(".kicad_sch") and fn[:-len(".kicad_sch")] not in SHEET_FILES + [NAME]:
            os.remove(os.path.join(OUTDIR, fn))
    write_footprints()
    used = []
    for s in SHEETS:
        for name, sym, value, nets in s["parts"]:
            k = (symbol_name(sym, M.PART_EXTRA[name][0]), sym, M.PART_EXTRA[name][0])
            if k[0] not in [u[0] for u in used]:
                used.append(k)
    prefix = lambda sym: getattr(M.SYMBOLS[sym], "lib", {}).get("prefix") if getattr(M.SYMBOLS[sym], "lib", None) \
        else M.PREFIX.get(sym, "U")
    embedded, library = [], ['(kicad_symbol_lib (version 20231120) (generator "nibbles") (generator_version "8.0")']
    for kname, sym, variant in used:
        embedded += symbol_text(kname, sym, variant, prefix(sym), True)
        library += ["  " + l for l in symbol_text(kname, sym, variant, prefix(sym), False)]
    open(os.path.join(OUTDIR, "nibbles.kicad_sym"), "w").write("\n".join(library + [")"]) + "\n")
    root_uuid = uid("root") if BOARD == "power" else uid("root", BOARD)
    sheet_uuids = [uid("sheet", n) for n in SHEET_NAMES]
    for i, s in enumerate(SHEETS):
        write_sheet(i, s, root_uuid, sheet_uuids[i], embedded)
    write_root(root_uuid, sheet_uuids)
    write_project()
    nparts, nnets = write_board(root_uuid, sheet_uuids)
    patch_project()  # saving the board rewrote the project file with KiCad's defaults
    print(f"{nparts} footprints, {nnets} nets -> {os.path.relpath(OUTDIR, REPO)}/{NAME}.kicad_pro")


if __name__ == "__main__":
    main()
