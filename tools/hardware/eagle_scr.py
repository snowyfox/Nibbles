"""Reads an EAGLE library script (.scr), as Ultra Librarian exports them
("Eagle6 Library"), into packages, symbols and devices that
make_power_board_sch.py writes into its schematic's library.

Only what Ultra Librarian's scripts use is understood: Grid, Layer, Change
Size/Ratio/Drill, Smd, Pad, Wire (with arcs), Polygon, Text, Pin, Edit,
Prefix, Value, Add, Package, Technology, Connect, Attribute. Coordinates
come in mils and go out in millimetres. Text that is only notes for the
library maker (padstyle notes, dimensions; anything not >NAME or >VALUE)
and the Document layer (48) are left out.
"""
import re
from xml.sax.saxutils import escape

MIL = 0.0254
DIRECTION = {"pas": "pas", "pwr": "pwr", "in": "in", "out": "out", "i/o": "io", "io": "io", "oc": "oc",
             "hiz": "hiz", "sup": "sup", "nc": "nc"}


def f(v):
    return ("%.4f" % v).rstrip("0").rstrip(".")


def statements(text):
    """One statement per command; continuation lines (starting with a
    number or '(') join the one before. Pin lines have no ';'."""
    out = []
    for line in text.splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if re.match(r"^[-(0-9.]", line) and out:
            out[-1] += " " + line
        else:
            out.append(line)
    return [s.rstrip(";").strip() for s in out]


def points(s):
    return [(float(x) * MIL, float(y) * MIL) for x, y in re.findall(r"\(\s*(-?[\d.]+)\s+(-?[\d.]+)\s*\)", s)]


class Library:
    def __init__(self, path):
        self.packages, self.symbols, self.devicesets = {}, {}, {}
        self.pkgdata = {}  # name -> {"smd": [...], "pad": [...], "wire": [...], "polygon": [...]}, millimetres
        self._read(open(path, encoding="latin-1").read())

    def _read(self, text):
        kind = name = None
        layer, size, ratio, drill = 94, 1.778, 8, 0.0
        cur_dev = cur_variant = None
        for st in statements(text):
            word = st.split()[0].lower()
            if word == "edit":
                full = re.search(r"'([^']+)'", st).group(1)
                name, ext = full.rsplit(".", 1)
                kind = {"pac": "package", "sym": "symbol", "dev": "device"}[ext.lower()]
                if kind == "package":
                    self.packages[name] = []
                    self.pkgdata[name] = {"smd": [], "pad": [], "wire": [], "polygon": []}
                elif kind == "symbol":
                    self.symbols[name] = {"items": [], "pins": []}
                else:
                    self.devicesets[name] = {"prefix": "U", "gates": [], "devices": []}
                    cur_dev = self.devicesets[name]
            elif word == "layer":
                layer = int(st.split()[1])
            elif word == "change":
                _, what, val = st.split()[:3]
                if what.lower() == "size":
                    size = float(val) * MIL
                elif what.lower() == "ratio":
                    ratio = int(float(val))
                elif what.lower() == "drill":
                    drill = float(val) * MIL
            elif word == "smd" and kind == "package":
                m = re.match(r"Smd\s+'([^']+)'\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(-?[\d.]+)\s+(\S+)(.*?)\((.*)\)", st, re.I)
                pname, dx, dy, rnd, rot, flags = m.group(1), float(m.group(2)), float(m.group(3)), abs(float(m.group(4))), m.group(5), m.group(6)
                x, y = points(st)[0]
                extra = "".join(f' {fl.lower()}="no"' for fl in ("Stop", "Cream", "Thermals") if f"NO{fl.upper()}" in flags.upper())
                self.pkgdata[name]["smd"].append({"name": pname, "x": x, "y": y, "dx": dx * MIL, "dy": dy * MIL,
                                                  "rot": rot, "roundness": int(rnd), "layer": layer,
                                                  "nostop": "NOSTOP" in flags.upper(), "nocream": "NOCREAM" in flags.upper()})
                self.packages[name].append(
                    f'<smd name="{escape(pname)}" x="{f(x)}" y="{f(y)}" dx="{f(dx * MIL)}" dy="{f(dy * MIL)}" '
                    f'layer="{layer}" roundness="{int(rnd)}" rot="{rot}"{extra}/>')
            elif word == "pad" and kind == "package":
                m = re.match(r"Pad\s+'([^']+)'\s+(-?[\d.]+)\s+(\S+)\s+(\S+)", st, re.I)
                x, y = points(st)[0]
                shape = m.group(3).lower()
                self.pkgdata[name]["pad"].append({"name": m.group(1), "x": x, "y": y, "drill": drill,
                                                  "diameter": float(m.group(2)) * MIL, "shape": shape, "rot": m.group(4)})
                self.packages[name].append(
                    f'<pad name="{escape(m.group(1))}" x="{f(x)}" y="{f(y)}" drill="{f(drill)}" '
                    f'diameter="{f(float(m.group(2)) * MIL)}" shape="{shape}" rot="{m.group(4)}"/>')
            elif word == "wire":
                if layer == 48:
                    continue
                width = float(st.split()[1]) * MIL
                # points with optional curve values between them
                toks = re.findall(r"\(\s*-?[\d.]+\s+-?[\d.]+\s*\)|-?[\d.]+", st[st.index("("):])
                pts, curves, pending = [], [], None
                for t in toks:
                    if t.startswith("("):
                        if pts:
                            curves.append(pending)
                        pts.append(points(t)[0])
                        pending = None
                    else:
                        pending = float(t)
                items = self._items(kind, name)
                if kind == "package":
                    for i in range(len(pts) - 1):
                        self.pkgdata[name]["wire"].append({"p1": pts[i], "p2": pts[i + 1], "curve": curves[i],
                                                           "width": width, "layer": layer})
                for i in range(len(pts) - 1):
                    c = f' curve="{f(curves[i])}"' if curves[i] else ""
                    (x1, y1), (x2, y2) = pts[i], pts[i + 1]
                    items.append(f'<wire x1="{f(x1)}" y1="{f(y1)}" x2="{f(x2)}" y2="{f(y2)}" '
                                 f'width="{f(width)}" layer="{layer}"{c}/>')
            elif word == "polygon":
                if layer == 48:
                    continue
                width = float(st.split()[1]) * MIL
                pts = points(st)
                if len(pts) > 1 and pts[0] == pts[-1]:
                    pts = pts[:-1]
                if kind == "package":
                    self.pkgdata[name]["polygon"].append({"pts": pts, "width": width, "layer": layer})
                self._items(kind, name).append(
                    f'<polygon width="{f(width)}" layer="{layer}">'
                    + "".join(f'<vertex x="{f(x)}" y="{f(y)}"/>' for x, y in pts) + "</polygon>")
            elif word == "text":
                t = re.search(r"'([^']*)'", st).group(1)
                if not t.startswith(">") or layer == 48:
                    continue
                x, y = points(st)[0]
                rot = re.search(r"'\s+(\S+)\s+\(", st).group(1).lstrip("S")
                r_ = f' rot="{rot}"' if rot not in ("R0", "") else ""
                self._items(kind, name).append(
                    f'<text x="{f(x)}" y="{f(y)}" size="{f(size)}" layer="{layer}" ratio="{ratio}"{r_}>'
                    f'{escape(t.upper())}</text>')
            elif word == "pin" and kind == "symbol":
                m = re.match(r"Pin\s+'([^']+)'\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)", st, re.I)
                pname, d, func, length, rot, vis, swap = m.groups()
                x, y = points(st)[0]
                self.symbols[name]["pins"].append({
                    "name": pname, "x": x, "y": y, "rot": rot, "length": length.lower(),
                    "direction": DIRECTION.get(d.lower(), "pas"), "visible": vis.lower(),
                    "function": func.lower(), "swaplevel": swap})
            elif word == "prefix" and kind == "device":
                cur_dev["prefix"] = re.search(r"'([^']*)'", st).group(1)
            elif word == "add" and kind == "device":
                m = re.match(r"Add\s+(\S+)\s+'([^']+)'", st, re.I)
                x, y = points(st)[0]
                cur_dev["gates"].append((m.group(2), m.group(1), x, y))
            elif word == "package" and kind == "device":
                names = re.findall(r"'([^']*)'", st)
                cur_variant = {"package": names[0], "name": names[1] if len(names) > 1 else "", "connects": []}
                cur_dev["devices"].append(cur_variant)
            elif word == "connect" and kind == "device":
                gp, pads = re.findall(r"'([^']*)'", st)[:2]
                gate, pin = gp.split(".", 1)
                cur_variant["connects"].append((gate, pin, pads))

    def _items(self, kind, name):
        return self.packages[name] if kind == "package" else self.symbols[name]["items"]

    # ------------------------------------------------------------ XML
    def symbol_xml(self, name, as_name=None):
        s = self.symbols[name]
        out = [f'<symbol name="{escape(as_name or name)}">'] + s["items"]
        for p in s["pins"]:
            func = f' function="{p["function"]}"' if p["function"] not in ("none", "") else ""
            out.append(f'<pin name="{escape(p["name"])}" x="{f(p["x"])}" y="{f(p["y"])}" '
                       f'visible="{p["visible"]}" length="{p["length"]}" direction="{p["direction"]}"{func} '
                       f'swaplevel="{p["swaplevel"]}" rot="{p["rot"]}"/>')
        return out + ["</symbol>"]

    def package_xml(self, name):
        return [f'<package name="{escape(name)}">'] + self.packages[name] + ["</package>"]
