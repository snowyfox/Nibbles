#!/usr/bin/env python3
"""Writes where each LED goes on a preview picture, a side view of the shark
from the simulator's layout: firmware/preview/main/led_pos.h and
firmware/base/main/shark_pos.h.

    python3 tools/sim/make_preview_map.py
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..", "..")
layout = json.load(open(os.path.join(HERE, "web", "layout.json")))
leds = layout["leds"]
us = [p[1] for p in leds]
vs = [p[2] for p in leds]


def write(path, name, w, h, margin):
    scale = min((w - 2 * margin) / (max(us) - min(us)), (h - 2 * margin) / (max(vs) - min(vs)))
    ox = (w - (max(us) - min(us)) * scale) / 2
    oy = (h - (max(vs) - min(vs)) * scale) / 2
    pos = [(round((p[1] - min(us)) * scale + ox), round((max(vs) - p[2]) * scale + oy)) for p in leds]
    with open(os.path.join(ROOT, path), "w") as f:
        f.write("// Made by tools/sim/make_preview_map.py from tools/sim/web/layout.json.\n#pragma once\n#include <stdint.h>\n\n")
        f.write(f"#define LED_POS_COUNT {len(pos)}\n")
        f.write(f"static const uint16_t {name}[LED_POS_COUNT][2] = {{\n")
        for i in range(0, len(pos), 8):
            f.write("    " + " ".join("{%d,%d}," % q for q in pos[i:i + 8]) + "\n")
        f.write("};\n")
    print(f"{path}: {len(pos)} LEDs at {scale:.3f} px/mm")


write("firmware/preview/main/led_pos.h", "led_pos", 536, 240, 6)     # the preview screen
write("firmware/base/main/shark_pos.h", "shark_pos", 164, 82, 2)    # the base's main page


def dot3(a, b):
    return sum(x * y for x, y in zip(a, b))


def fins(path, w, h, gap, margin):
    """The fins laid flat, seen from above: leading edges up, roots towards the
    middle, port on the left. a runs forward along the hinge, b out from it."""
    flat = {}
    for hinge in layout["fin_hinges"]:
        ch = next(c for c in layout["channels"] if c["channel"] == hinge["channel"])
        turn = hinge["turn"] if hinge["turn"][1] > 0 else [-x for x in hinge["turn"]]   # forward
        pts = leds[ch["start"]:ch["start"] + ch["count"]]
        rel = lambda p: [p[i] - hinge["point"][i] for i in range(3)]

        def perp(p):
            q = rel(p)
            k = dot3(q, turn)
            return [q[i] - k * turn[i] for i in range(3)]
        far = max((perp(p) for p in pts), key=lambda v: dot3(v, v))
        n = dot3(far, far) ** 0.5
        span = [x / n for x in far]
        flat[hinge["side"]] = {"ch": ch, "outline": [(dot3(rel(p), turn), dot3(rel(p), span)) for p in layout["outlines"][hinge["outline"]]],
                               "leds": [(dot3(rel(p), turn), dot3(rel(p), span)) for p in pts]}
    every = [q for f in flat.values() for q in f["leds"] + f["outline"]]
    a0, a1 = min(q[0] for q in every), max(q[0] for q in every)
    b0, b1 = min(q[1] for q in every), max(q[1] for q in every)
    scale = min((w - gap - 2 * margin) / 2 / (b1 - b0), (h - 2 * margin) / (a1 - a0))
    oy = (h - (a1 - a0) * scale) / 2

    def place(side, q):
        d = (q[1] - b0) * scale
        return (w / 2 - gap / 2 - d if side == "port" else w / 2 + gap / 2 + d, (a1 - q[0]) * scale + oy)

    def seg_dist(p, a, b):
        dx, dy = b[0] - a[0], b[1] - a[1]
        k = max(0, min(1, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / (dx * dx + dy * dy or 1)))
        return ((p[0] - a[0] - k * dx) ** 2 + (p[1] - a[1] - k * dy) ** 2) ** 0.5

    first = min(f["ch"]["start"] for f in flat.values())
    count = sum(f["ch"]["count"] for f in flat.values())
    mask = {}
    for side, f in flat.items():
        # The strip runs out and back along each edge, so each edge has two
        # strands side by side. The band around the line between them is the
        # fin's shape; each strand moves in to OFFSET from that line, and every
        # pixel of the band takes its nearest LED, so both strands show, side
        # by side, and neighbouring LEDs touch.
        pts = f["leds"]
        n = len(pts)
        mids = []
        for i, q in enumerate(pts):
            others = [pts[j] for j in range(n) if min(abs(i - j), n - abs(i - j)) > 8]
            o = min(others, key=lambda r: (r[0] - q[0]) ** 2 + (r[1] - q[1]) ** 2)
            mids.append(((q[0] + o[0]) / 2, (q[1] + o[1]) / 2) if ((o[0] - q[0]) ** 2 + (o[1] - q[1]) ** 2) ** 0.5 * scale < 8 else q)
        centre = []
        for i in range(n):   # the strands' LEDs don't line up: smooth out the zigzag that leaves
            near = [mids[(i + k) % n] for k in range(-3, 4)]
            centre.append(place(side, (sum(q[0] for q in near) / 7, sum(q[1] for q in near) / 7)))
        moved = []
        for i, q in enumerate(pts):
            p = place(side, q)
            dx, dy = p[0] - centre[i][0], p[1] - centre[i][1]
            d = (dx * dx + dy * dy) ** 0.5
            moved.append((centre[i][0] + dx / d * OFFSET, centre[i][1] + dy / d * OFFSET) if d > 0.01 else centre[i])
        xs, ys = [c[0] for c in centre], [c[1] for c in centre]
        for y in range(max(0, int(min(ys)) - 3), min(h, int(max(ys)) + 4)):
            for x in range(max(0, int(min(xs)) - 3), min(w, int(max(xs)) + 4)):
                c = (x + 0.5, y + 0.5)
                if min(seg_dist(c, centre[i], centre[(i + 1) % n]) for i in range(n)) <= BAND / 2:
                    k = min(range(n), key=lambda i: (moved[i][0] - c[0]) ** 2 + (moved[i][1] - c[1]) ** 2)
                    mask[y * w + x] = f["ch"]["start"] - first + k
    with open(os.path.join(ROOT, path), "w") as o:
        o.write("// Made by tools/sim/make_preview_map.py from tools/sim/web/layout.json: the fins\n// laid flat, seen from above, leading edges up, port on the left. Each pixel of\n// the fins' shape and the LED (from FIN_FIRST_LED) that colours it.\n#pragma once\n#include <stdint.h>\n\n")
        o.write(f"#define FIN_W {w}\n#define FIN_H {h}\n#define FIN_FIRST_LED {first}\n#define FIN_PIXELS {len(mask)}\n")
        o.write("static const struct { uint16_t at; uint8_t led; } fin_mask[FIN_PIXELS] = {\n")
        items = sorted(mask.items())
        for i in range(0, len(items), 8):
            o.write("    " + " ".join("{%d,%d}," % q for q in items[i:i + 8]) + "\n")
        o.write("};\n")
    print(f"{path}: {count} LEDs from {first} at {scale:.3f} px/mm, {len(mask)} pixels")


BAND = 4.0     # the shark's line, as measured on screen
OFFSET = 1.0   # each strand from the middle of the band
fins("firmware/base/main/fin_pos.h", 156, 76, 26, 1)
