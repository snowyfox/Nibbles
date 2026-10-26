"""Reads a KiCad footprint (.kicad_mod, from KiCad's official library) into
an EAGLE package for make_power_board_sch.py.

KiCad's y axis points down and EAGLE's up, so y and rotations flip.
Handled: smd pads (rect, roundrect, oval, circle, custom: the anchor pad
plus its polygon on copper, stop and cream), plated holes (oval drills
become a round drill plus a milled slot), non-plated holes, and the
silkscreen and fab drawing (lines, arcs, circles, rectangles, polygons).
Pads that sit exactly on an earlier pad (USB-C's stacked A/B pins) are
merged into it; unnamed pads become M1, M2...; repeated names (MP, SH)
are numbered. `aliases` records what each original pad became.
"""
import math
import re
from xml.sax.saxutils import escape

LAYER = {"F.SilkS": 21, "F.Silkscreen": 21, "F.Fab": 51}


def f(v):
    return ("%.4f" % v).rstrip("0").rstrip(".")


def parse(text):
    """S-expression -> nested lists of strings."""
    toks = re.findall(r'\(|\)|"(?:[^"\\]|\\.)*"|[^\s()]+', text)
    stack, cur = [], []
    for t in toks:
        if t == "(":
            stack.append(cur)
            cur = []
        elif t == ")":
            done = cur
            cur = stack.pop()
            cur.append(done)
        else:
            cur.append(t[1:-1] if t.startswith('"') else t)
    return cur[0]


def find(node, key):
    return [c for c in node if isinstance(c, list) and c and c[0] == key]


def one(node, key):
    r = find(node, key)
    return r[0] if r else None


def xy(n):
    return float(n[1]), -float(n[2])


def rot_of(at):
    a = -float(at[3]) if len(at) > 3 else 0.0
    a %= 360
    return f"R{f(a)}" if a else "R0"


class Footprint:
    def __init__(self, path, name=None):
        self.root = parse(open(path, encoding="utf-8").read())
        self.name = name or self.root[1]
        self.items, self.aliases, self.pads = [], {}, []
        self._read()

    def _layer(self, node):
        lay = one(node, "layer")
        return LAYER.get(lay[1]) if lay else None

    def _read(self):
        seen_at, counts = {}, {}
        unnamed = 0
        for p in find(self.root, "pad"):
            name, kind, shape = p[1], p[2], p[3]
            x, y = xy(one(p, "at"))
            rot = rot_of(one(p, "at"))
            size = one(p, "size")
            sx, sy = float(size[1]), float(size[2])
            if kind == "np_thru_hole":
                d = one(p, "drill")
                self.items.append(f'<hole x="{f(x)}" y="{f(y)}" drill="{f(float(d[1]))}"/>')
                continue
            if not name:
                unnamed += 1
                name = f"M{unnamed}"
            key = (round(x, 3), round(y, 3), kind)
            if key in seen_at:  # stacked on an earlier pad: one pad
                self.aliases.setdefault(name, []).append(seen_at[key])
                continue
            orig = name
            counts[name] = counts.get(name, 0) + 1
            total = sum(1 for q in find(self.root, "pad") if q[1] == orig)
            if total > 1 and orig:
                name = f"{orig}{counts[orig]}"
            seen_at[key] = name
            self.aliases.setdefault(orig or name, []).append(name)
            self.pads.append(name)
            if kind == "smd":
                rnd = 0
                if shape in ("circle", "oval"):
                    rnd = 100
                elif shape == "roundrect":
                    rr = one(p, "roundrect_rratio")
                    rnd = min(100, int(round(float(rr[1]) * 200))) if rr else 25
                self.items.append(f'<smd name="{escape(name)}" x="{f(x)}" y="{f(y)}" dx="{f(sx)}" dy="{f(sy)}" '
                                  f'layer="1" roundness="{rnd}" rot="{rot}"/>')
                prim = one(p, "primitives")
                if shape == "custom" and prim:
                    for gp in find(prim, "gr_poly"):
                        pts = [(float(v[1]), -float(v[2])) for v in find(one(gp, "pts"), "xy")]
                        for layer in (1, 29, 31):
                            self.items.append(f'<polygon width="0" layer="{layer}">' + "".join(
                                f'<vertex x="{f(x + px)}" y="{f(y + py)}"/>' for px, py in pts) + "</polygon>")
            else:  # plated through hole
                d = one(p, "drill")
                if d[1] == "oval":  # slot: round drill + milled outline
                    dx, dy = float(d[2]), float(d[3])
                    drill = min(dx, dy)
                    self._slot(x, y, dx, dy)
                else:
                    drill = float(d[1])
                if shape in ("rect", "roundrect"):
                    eshape, dia, r_ = "square", min(sx, sy), rot
                elif shape == "oval" and abs(sx - sy) > 1e-6:
                    eshape, dia = "long", min(sx, sy)
                    r_ = "R90" if sy > sx else "R0"
                else:
                    eshape, dia, r_ = "round", max(sx, sy), rot
                self.items.append(f'<pad name="{escape(name)}" x="{f(x)}" y="{f(y)}" drill="{f(drill)}" '
                                  f'diameter="{f(dia)}" shape="{eshape}" rot="{r_}"/>')
        for g in find(self.root, "fp_line"):
            layer = self._layer(g)
            if layer:
                (x1, y1), (x2, y2) = xy(one(g, "start")), xy(one(g, "end"))
                self.items.append(self._wire(x1, y1, x2, y2, g, layer))
        for g in find(self.root, "fp_rect"):
            layer = self._layer(g)
            if layer:
                (x1, y1), (x2, y2) = xy(one(g, "start")), xy(one(g, "end"))
                for a, b in (((x1, y1), (x2, y1)), ((x2, y1), (x2, y2)), ((x2, y2), (x1, y2)), ((x1, y2), (x1, y1))):
                    self.items.append(self._wire(*a, *b, g, layer))
        for g in find(self.root, "fp_poly"):
            layer = self._layer(g)
            if layer:
                pts = [xy(v) for v in find(one(g, "pts"), "xy")]
                for a, b in zip(pts, pts[1:] + pts[:1]):
                    self.items.append(self._wire(*a, *b, g, layer))
        for g in find(self.root, "fp_circle"):
            layer = self._layer(g)
            if layer:
                (cx, cy), (ex, ey) = xy(one(g, "center")), xy(one(g, "end"))
                self.items.append(f'<circle x="{f(cx)}" y="{f(cy)}" radius="{f(math.hypot(ex - cx, ey - cy))}" '
                                  f'width="{f(self._width(g))}" layer="{layer}"/>')
        for g in find(self.root, "fp_arc"):
            layer = self._layer(g)
            if layer:
                s, m, e = xy(one(g, "start")), xy(one(g, "mid")), xy(one(g, "end"))
                self.items.append(self._wire(*s, *e, g, layer, curve=arc_angle(s, m, e)))
        ys = [float(a) for a in re.findall(r"\(at\s+-?[\d.]+\s+(-?[\d.]+)", str(self.root))] or [0]
        top, bottom = -min(ys) + 1.5, -max(ys) - 1.5
        self.items.append(f'<text x="0" y="{f(top)}" size="1" layer="25" align="bottom-center">&gt;NAME</text>')
        self.items.append(f'<text x="0" y="{f(bottom)}" size="1" layer="27" align="top-center">&gt;VALUE</text>')

    def _width(self, g):
        st = one(g, "stroke")
        w = one(st, "width") if st else one(g, "width")
        return float(w[1]) if w else 0.12

    def _wire(self, x1, y1, x2, y2, g, layer, curve=None):
        c = f' curve="{f(curve)}"' if curve else ""
        return (f'<wire x1="{f(x1)}" y1="{f(y1)}" x2="{f(x2)}" y2="{f(y2)}" width="{f(self._width(g))}" '
                f'layer="{layer}"{c}/>')

    def _slot(self, x, y, dx, dy):
        """Milled outline (layer 46) of an oval hole."""
        r = min(dx, dy) / 2
        h = (max(dx, dy) - min(dx, dy)) / 2
        if dy >= dx:
            a, b = (x - r, y - h), (x - r, y + h)
            c, d = (x + r, y + h), (x + r, y - h)
        else:
            a, b = (x - h, y + r), (x + h, y + r)
            c, d = (x + h, y - r), (x - h, y - r)
        w = 0
        self.items += [f'<wire x1="{f(a[0])}" y1="{f(a[1])}" x2="{f(b[0])}" y2="{f(b[1])}" width="{w}" layer="46"/>',
                       f'<wire x1="{f(b[0])}" y1="{f(b[1])}" x2="{f(c[0])}" y2="{f(c[1])}" width="{w}" layer="46" curve="-180"/>',
                       f'<wire x1="{f(c[0])}" y1="{f(c[1])}" x2="{f(d[0])}" y2="{f(d[1])}" width="{w}" layer="46"/>',
                       f'<wire x1="{f(d[0])}" y1="{f(d[1])}" x2="{f(a[0])}" y2="{f(a[1])}" width="{w}" layer="46" curve="-180"/>']

    def pads_for(self, names):
        """Original KiCad pad names -> this package's pad names."""
        out = []
        for n in names:
            for p in self.aliases.get(n, [n]):
                if p not in out:
                    out.append(p)
        return out

    def xml(self, name=None):
        return [f'<package name="{escape(name or self.name)}">'] + self.items + ["</package>"]


def arc_angle(s, m, e):
    """Signed sweep (degrees, EAGLE's counter-clockwise positive) of the arc s->m->e."""
    ax, ay = s
    bx, by = m
    cx, cy = e
    d = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by))
    if abs(d) < 1e-12:
        return None
    ux = ((ax * ax + ay * ay) * (by - cy) + (bx * bx + by * by) * (cy - ay) + (cx * cx + cy * cy) * (ay - by)) / d
    uy = ((ax * ax + ay * ay) * (cx - bx) + (bx * bx + by * by) * (ax - cx) + (cx * cx + cy * cy) * (bx - ax)) / d
    a0 = math.atan2(ay - uy, ax - ux)
    a1 = math.atan2(by - uy, bx - ux)
    a2 = math.atan2(cy - uy, cx - ux)
    ccw = (a1 - a0) % (2 * math.pi) < (a2 - a0) % (2 * math.pi)
    sweep = (a2 - a0) % (2 * math.pi)
    return math.degrees(sweep if ccw else sweep - 2 * math.pi)
