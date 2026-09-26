#!/usr/bin/env python3
"""Builds the simulator's LED layout from the Fusion 360 geometry export.

    python3 tools/sim/make_layout.py      # reads tools/fusion/output/geometry.json
                                          # writes tools/sim/web/layout.json

How the shark is wired (confirmed on the shark on 2026-09-26 by lighting
each channel's first LEDs):
- Channels 1 and 2 (418 and 417 LEDs): two strips back to back inside the
  main tube, one facing into the body and one facing out. Both start at the
  back edge of the pole mount and run all the way round the shark's outline.
- Channels 3 and 4 (114 and 115 LEDs): port and starboard fin. The strip is
  folded in half inside the fin tube: it starts at the fin's leading (front)
  edge on the outward-facing side, runs to the other end of the tube, and
  comes back on the inward-facing side.
- Channel 5 (130 LEDs): last year's eye lights in the eye opening. They are
  not in the model, so they are drawn as a ring on each side of the head.

Positions are in mm in the design's coordinates (Z up, +Y towards the nose,
+X starboard). The two strips sharing a tube are drawn OFFSET_MM either side
of its centre line so both can be seen.
"""
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
GEOMETRY = os.path.join(HERE, "..", "fusion", "output", "geometry.json")
MESH_IN = os.path.join(HERE, "..", "fusion", "output", "shark.obj")    # Fusion exports OBJ in cm
MESH_OUT = os.path.join(HERE, "web", "shark.obj")
LAYOUT = os.path.join(HERE, "web", "layout.json")

SETTINGS = {
    "body_first_towards": "tail",     # channels 1/2 leave the pole mount heading to the tail
    "body_outward_channel": 1,        # channel 1 faces out, channel 2 into the body
    "starboard_fin_channel": 4,       # channel 3 is the port fin
    "fin_start": "front",             # fin strips start at the leading edge ("front") end of the tube
    "fin_first_half_outward": True,   # the fin strip's first half faces out
    "leds": {1: 418, 2: 417, 3: 114, 4: 115, 5: 130},
    "pole_socket_radius_mm": 12.7,
    "offset_mm": 3.0,
    "eye_ring_radius_mm": 21.0,
    "eye_ring_x_mm": 12.0,            # the two rings, either side of the centre plane
    "hidden_channels": [5],           # not drawn: the old eye lights, now behind the eye screens
    "eye_screen_radius_mm": 22.2,     # the eyes' round 1.75" AMOLED screens
    "eye_screen_x_mm": 17.25,         # each screen face from the body's centre plane (planned mount)
}


def sub(a, b): return [a[i] - b[i] for i in range(3)]
def add(a, b): return [a[i] + b[i] for i in range(3)]
def mul(a, k): return [v * k for v in a]
def dot(a, b): return sum(a[i] * b[i] for i in range(3))
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
def norm(a):
    n = math.sqrt(dot(a, a))
    return [v / n for v in a] if n > 1e-9 else [0.0, 0.0, 0.0]
def dist(a, b): return math.sqrt(dot(sub(a, b), sub(a, b)))


def length(line):
    return sum(dist(line[i], line[i + 1]) for i in range(len(line) - 1))


def point_and_tangent(line, s):
    for i in range(len(line) - 1):
        d = dist(line[i], line[i + 1])
        if s <= d or i == len(line) - 2:
            f = 0.0 if d == 0 else min(max(s / d, 0.0), 1.0)
            return add(line[i], mul(sub(line[i + 1], line[i]), f)), norm(sub(line[i + 1], line[i]))
        s -= d
    return line[-1], norm(sub(line[-1], line[-2]))


def plane_normal(line):
    """Normal of the plane a (roughly planar) line lies in."""
    c = mul([sum(p[k] for p in line) for k in range(3)], 1.0 / len(line))
    n = [0.0, 0.0, 0.0]
    for i in range(len(line) - 1):  # Newell's method around the centroid
        n = add(n, cross(sub(line[i], c), sub(line[i + 1], c)))
    return norm(n), c


def spread(line, count, offset_mm, outward, closed=False):
    """count LEDs evenly along line, each shifted offset_mm off the centre line within
    the line's plane: outward or inward. For a closed loop "outward" is outside the
    loop everywhere (from its winding, so it stays right along inward curves); for
    an open line it is away from the line's centre."""
    n, c = plane_normal(line)
    total = length(line)
    out = []
    for i in range(count):
        p, t = point_and_tangent(line, total * (i + 0.5) / count)
        side = norm(cross(t, n))              # in the plane, across the tube; outside for a
                                              # loop wound anticlockwise about n (Newell's n is)
        if not closed and dot(side, sub(p, c)) < 0:
            side = mul(side, -1.0)            # make it point away from the centre
        out.append(add(p, mul(side, offset_mm if outward else -offset_mm)))
    return out


def seg_cross_2d(a, b, c, d):
    """Where segments ab and cd cross (2D), as the parameter along ab, or None."""
    r = (b[0] - a[0], b[1] - a[1]); q = (d[0] - c[0], d[1] - c[1])
    den = r[0] * q[1] - r[1] * q[0]
    if abs(den) < 1e-12:
        return None
    t = ((c[0] - a[0]) * q[1] - (c[1] - a[1]) * q[0]) / den
    u = ((c[0] - a[0]) * r[1] - (c[1] - a[1]) * r[0]) / den
    return t if 0 <= t <= 1 and 0 <= u <= 1 else None


def cut_corner_loops(loop, max_loop_mm=80.0):
    """At sharp corners the traced centre line runs on into the mitred tip where two
    tube pieces meet, and comes back across itself: a small loop. Cut each loop at
    the crossing, leaving a clean corner. loop is closed (last point == first)."""
    n, c = plane_normal(loop)
    u = norm(sub(loop[1], loop[0]))
    u = norm(sub(u, mul(n, dot(u, n))))
    v = cross(n, u)
    pts = loop[:-1]
    cuts = 0
    changed = True
    while changed:
        changed = False
        flat = [(dot(sub(p, c), u), dot(sub(p, c), v)) for p in pts]
        m = len(pts)
        for i in range(m):
            run = 0.0
            for k in range(2, m - 1):
                j = (i + k) % m
                run += dist(pts[(j - 1) % m], pts[j])
                if run > max_loop_mm:
                    break
                t = seg_cross_2d(flat[i], flat[(i + 1) % m], flat[j], flat[(j + 1) % m])
                if t is None:
                    continue
                x = add(pts[i], mul(sub(pts[(i + 1) % m], pts[i]), t))
                drop = {(i + d) % m for d in range(1, k + 1)}   # the loop: points i+1 .. j
                keep = [p for idx, p in enumerate(pts) if idx not in drop]
                keep.insert(keep.index(pts[i]) + 1, x)
                pts = keep
                cuts += 1
                changed = True
                break
            if changed:
                break
    return pts + [pts[0]], cuts


def main():
    g = json.load(open(GEOMETRY))
    S = SETTINGS
    tubes = [t for t in g["tubes"] if "line" in t]
    body = max(tubes, key=lambda t: t["length_mm"])                       # the main outline tube
    fins = sorted([t for t in tubes if 300 < t["length_mm"] < 500 and abs(t["start"][0]) > 5],
                  key=lambda t: -t["start"][0])                           # starboard (+X) first
    pole = next(c for c in g["cylinders"] if "CF Tube" in c["component"])

    # ---- channels 1 and 2: round the main tube from the back edge of the pole mount
    loop, cuts = cut_corner_loops(body["line"] + [body["line"][0]])     # closed
    pole_y = pole["from"][1]
    target_y = pole_y - S["pole_socket_radius_mm"]                        # back edge: towards the tail (-Y)
    lowest = min(p[2] for p in loop)
    belly = [i for i, p in enumerate(loop[:-1]) if p[2] < lowest + 80]    # the underside of the outline
    start = min(belly, key=lambda i: abs(loop[i][1] - target_y))
    ring = loop[start:-1] + loop[:start + 1]                              # starts (and ends) at the mount
    # which way is the tail? the next few mm either go to -Y (tail) or +Y (nose)
    heads_to_tail = ring[5][1] < ring[0][1]
    if heads_to_tail != (S["body_first_towards"] == "tail"):
        ring = list(reversed(ring))

    leds, channels = [], []

    def add_channel(ch, points, note):
        channels.append({"channel": ch, "start": len(leds), "count": len(points), "note": note,
                         "hidden": ch in S["hidden_channels"]})
        leds.extend([[round(v, 2) for v in p] for p in points])

    for ch in (1, 2):
        outward = ch == S["body_outward_channel"]
        add_channel(ch, spread(ring, S["leds"][ch], S["offset_mm"], outward, closed=True),
                    "main tube, %s-facing strip, from the pole mount towards the %s"
                    % ("out" if outward else "in", S["body_first_towards"]))

    # ---- channels 3 and 4: fins, folded strips starting and ending at the stern end
    port_ch = 4 if S["starboard_fin_channel"] == 3 else 3
    fin_channels = [(fins[0], S["starboard_fin_channel"], "starboard"), (fins[1], port_ch, "port")]
    for fin, ch, side in sorted(fin_channels, key=lambda f: f[1]):  # in WLED's LED order
        line = fin["line"]
        starts_at_front = line[0][1] > line[-1][1]                        # +Y is towards the nose
        if starts_at_front != (S["fin_start"] == "front"):
            line = list(reversed(line))
        n = S["leds"][ch]
        first = n // 2 + n % 2
        add_channel(ch, spread(line, first, S["offset_mm"], S["fin_first_half_outward"])
                    + spread(list(reversed(line)), n - first, S["offset_mm"], not S["fin_first_half_outward"]),
                    "%s fin, folded: from the %s end, out-facing half first" % (side, S["fin_start"]))

    # ---- channel 5: last year's eye lights, a ring each side of the eye opening
    hole = max((c for c in g["cylinders"] if "SharkBody" in c["component"] and abs(c["axis"][0]) > 0.99
                and c["radius_mm"] < 40), key=lambda c: c["radius_mm"])
    centre = [0.0, hole["from"][1], hole["from"][2]]
    n5 = S["leds"][5]
    ring_pts = []
    for side, count in ((1, n5 // 2), (-1, n5 - n5 // 2)):
        for i in range(count):
            a = 2 * math.pi * i / count
            ring_pts.append([side * S["eye_ring_x_mm"], centre[1] + S["eye_ring_radius_mm"] * math.cos(a),
                             centre[2] + S["eye_ring_radius_mm"] * math.sin(a)])
    add_channel(5, ring_pts, "old eye lights (not modelled): a ring each side of the eye opening")

    # ---- the eyes: a round screen each side, centred on the eye opening, facing out.
    # "right" is the screen's +x: towards the nose on starboard, the tail on port
    # (as motion_analysis_set_mount assumes).
    eyes = [{"side": side, "centre": [sx * S["eye_screen_x_mm"], centre[1], centre[2]],
             "normal": [sx, 0.0, 0.0], "right": [0.0, float(sx), 0.0], "radius_mm": S["eye_screen_radius_mm"]}
            for side, sx in (("starboard", 1), ("port", -1))]

    def thin(line, step):
        out, run = [line[0]], 0.0
        for i in range(1, len(line)):
            run += dist(line[i - 1], line[i])
            if run >= step:
                out.append([round(v, 1) for v in line[i]])
                run = 0.0
        return out

    layout = {
        "name": "shark (Fusion 360 export)",
        "units": "mm",
        "up": g.get("up", "z"),
        "leds": leds,
        "channels": channels,
        "outlines": [thin(loop, 4.0)] + [thin(f["line"], 4.0) for f in fins],
        "eyes": eyes,
        "pole": {"from": pole["from"], "to": pole["to"], "radius_mm": pole["radius_mm"]},
        "mesh": {"file": "shark.obj", "scale": 10.0} if os.path.exists(MESH_IN) else None,
        "settings": {k: v for k, v in S.items() if k != "leds"},
    }
    with open(LAYOUT, "w") as f:
        json.dump(layout, f)
    if os.path.exists(MESH_IN):  # the shark's shape for the 3D view (material library dropped)
        with open(MESH_IN) as src, open(MESH_OUT, "w") as dst:
            dst.writelines(l for l in src if not l.startswith(("mtllib", "usemtl")))
    for c in channels:
        print("channel %d: LEDs %d-%d  %s" % (c["channel"], c["start"], c["start"] + c["count"] - 1, c["note"]))
    print("%d LEDs -> %s" % (len(leds), os.path.relpath(LAYOUT)))
    print("main loop: %d corner loop(s) cut" % cuts)
    print("main loop %.0f mm (%.1f mm per LED), fins %.0f mm, pole at Y %.1f, loop starts at %s"
          % (length(loop), length(loop) / S["leds"][1], fins[0]["length_mm"], pole_y, [round(v) for v in ring[0]]))


if __name__ == "__main__":
    main()
