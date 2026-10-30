#!/usr/bin/env python3
"""Builds a WLED 2D LED map of the shark from the simulator's layout.

    python3 tools/sim/make_ledmap.py [--cells 1500] [--fin-panel-mm 20]
        reads  tools/sim/web/layout.json
        writes wled/ledmap/ledmap.json (upload to WLED as /ledmap.json, then reboot)
               wled/ledmap/ledmap_preview.svg

The matrix is the shark seen from starboard: nose to the right, top row at the
top. WLED maps each matrix cell to one LED (-1: empty), but in this view LEDs
overlap: the two strips back to back in the main tube, and the port and
starboard fins, which cover each other. So only one of each is mapped where
it is (the outward body strip and the starboard fin); their partners (the
inward strip, the port fin) get spare cells nearby, and the Nibbles usermod
copies each partner LED's colour from the LED it pairs with after every frame
(the "copy" list in the file, which WLED itself ignores). Without the usermod
the partners still show roughly the right colours from their nearby cells.

Fins that follow their real angle: the "fingeo" list (also ignored by WLED)
gives the usermod what it needs to turn a fin's LEDs about its hinge and find
the cell each one is over now, so the fin shows the side view's colours
wherever it really is (with a fin sensor node; see NL_MSG_FIN_POSE):
  [cell_mm, u0, v1, ox, oy, fin_count,
   then per fin: side (0 port, 1 starboard), first LED, LED count,
                 hinge point x y z, turn axis x y z, modelled angle (deg),
   then every fin LED's rest position x y z (design mm, fins in that order)]
A position (y, z) is over cell (round((y - u0 + ox) / cell - 0.5),
round((v1 - z + oy) / cell - 0.5)), as for the mapped LEDs; turning a fin to
calibrated angle a is a turn of (modelled - a) degrees about the turn axis.

Fin panel (the fins' own segment): a region to the right of the side view,
--fin-panel-mm cells, with no LEDs of its own. Its first cell is mapped to the
LED with the last cell of all (its own cell comes later, and WLED writes the
cells to the LEDs in order, so that LED always shows its own cell): WLED
works out what a segment can do (colours, palettes) from the LEDs under it,
and a segment over no LEDs at all is an on/off one, whose effects draw in
segment colour 1 or not at all. A preset that switches a
segment on there gives the fins their own effect: the usermod places each fin
LED in the panel by its hinge angle and shows the panel's colour there, in
one of two projections (set per preset, {"nibbles":{"finproj":"side"|"top"}}):
side, the fins seen from starboard (both fins land on the same cells at equal
angles), or top, seen from above with the nose to the right (port in the
upper half, starboard in the lower). "finpanel" in the file (ignored by WLED):
  [px, py, pw, ph, mm_per_cell, side_y0, side_z1, top_y0, top_x0]
A position is at panel cell (px + (y - side_y0) / mm, py + (side_z1 - z) / mm)
seen from the side, or (px + (y - top_y0) / mm, py + (x - top_x0) / mm) from
above (cell centres at +0.5, as for the side view).

Memory: WLED keeps about 10 bytes per matrix cell (frame buffer, segment
buffer, map), so --cells sets the RAM cost: the default (1500, with 20 mm fin
panel cells) gives 68 x 33 = 2244 cells, ~21 KB, and every frame's cost scales
with the cell count (wled/bench/README.md). The shark's
controller has no PSRAM and ~72 KB free, so keep it modest and check the free
heap after loading. WLED limits each side to 255.

Loading /ledmap.json turns WLED into a 2D matrix at boot, for every preset:
1D presets (segments by strip position) need reworking for 2D.
"""
import argparse
import heapq
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
LAYOUT = os.path.join(HERE, "web", "layout.json")
OUT_DIR = os.path.join(REPO, "wled", "ledmap")


def sub(a, b): return [a[i] - b[i] for i in range(3)]
def dot(a, b): return sum(a[i] * b[i] for i in range(3))
def cross(a, b): return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
def norm(a):
    n = math.sqrt(dot(a, a))
    return [v / n for v in a]


def place(items, pos, grid, reach=None):
    """Put each LED in items in a free cell: every (LED, cell) pair cheapest
    first, so each cell goes to the nearest LED still unplaced and each LED to
    the nearest cell still free. Returns {led: distance moved, in cells}."""
    height, width = len(grid), len(grid[0])
    reach = reach or max(width, height)
    heap = []
    for i in items:
        x, y = pos[i]
        cx, cy = round(x), round(y)
        r = 2
        while True:  # widen the search until there are enough free cells in reach
            cells = [(gx, gy) for gx in range(max(0, cx - r), min(width, cx + r + 1))
                     for gy in range(max(0, cy - r), min(height, cy + r + 1)) if grid[gy][gx] == -1]
            if len(cells) >= 12 or r >= reach:
                break
            r *= 2
        for gx, gy in cells:
            heapq.heappush(heap, ((gx - x) ** 2 + (gy - y) ** 2, i, gx, gy))
    moved = {}
    while heap and len(moved) < len(items):
        d2, i, gx, gy = heapq.heappop(heap)
        if i in moved or grid[gy][gx] != -1:
            continue
        grid[gy][gx] = i
        moved[i] = math.sqrt(d2)
    missing = [i for i in items if i not in moved]
    if missing:  # crowded out of its candidate cells: nearest free cell anywhere
        for i in missing:
            x, y = pos[i]
            gx, gy = min(((gx, gy) for gy in range(height) for gx in range(width) if grid[gy][gx] == -1),
                         key=lambda c: (c[0] - x) ** 2 + (c[1] - y) ** 2)
            grid[gy][gx] = i
            moved[i] = math.hypot(gx - x, gy - y)
    return moved


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cells", type=int, default=1500, help="about how many matrix cells (RAM ~10 bytes each)")
    ap.add_argument("--fin-panel-mm", type=float, default=20.0, help="fin panel cell size (0: no fin panel)")
    args = ap.parse_args()

    layout = json.load(open(LAYOUT))
    assert layout.get("up", "z") == "z", "expects the design's coordinates (Z up)"
    leds = layout["leds"]
    ch = {c["channel"]: c for c in layout["channels"]}
    rng = lambda n: list(range(ch[n]["start"], ch[n]["start"] + ch[n]["count"]))
    s = layout["settings"]
    out_body = s["body_outward_channel"]
    in_body = 2 if out_body == 1 else 1
    sb_fin = s["starboard_fin_channel"]
    port_fin = s["port_fin_channel"]
    # (mapped channel, partner channel copied from it)
    pairs = [(out_body, in_body), (sb_fin, port_fin)]
    primary = rng(out_body) + rng(sb_fin)

    # Side view from starboard: u along +Y (nose, to the right), v along +Z (up).
    u0 = min(leds[i][1] for i in primary); u1 = max(leds[i][1] for i in primary)
    v0 = min(leds[i][2] for i in primary); v1 = max(leds[i][2] for i in primary)
    cell = math.sqrt((u1 - u0) * (v1 - v0) / args.cells)
    width = int((u1 - u0) / cell) + 1
    height = int((v1 - v0) / cell) + 1
    assert width <= 255 and height <= 255, "WLED allows at most 255 cells a side"
    ox = (width * cell - (u1 - u0)) / 2
    oy = (height * cell - (v1 - v0)) / 2
    pos = {i: ((leds[i][1] - u0 + ox) / cell - 0.5, (v1 - leds[i][2] + oy) / cell - 0.5) for i in range(len(leds))}

    grid = [[-1] * width for _ in range(height)]
    moved = place(primary, pos, grid)
    # Partners: the LED at the same fraction along its channel, and a spare
    # cell next to where that LED went.
    copy, partner_of = [], {}
    for src_ch, dst_ch in pairs:
        src, dst = rng(src_ch), rng(dst_ch)
        copy.append([dst[0], len(dst), src[0], len(src)])
        for k, d in enumerate(dst):
            partner_of[d] = src[round(k * (len(src) - 1) / max(1, len(dst) - 1))]
    at = {grid[y][x]: (x, y) for y in range(height) for x in range(width) if grid[y][x] != -1}
    ppos = {d: at[p] for d, p in partner_of.items()}
    pmoved = place(list(partner_of), ppos, grid, reach=8)

    # Extra rows at the bottom so a fin swung straight down still lands on the
    # grid. Added below only, so every existing cell keeps its index.
    def turned(p, c, k, a):
        v = sub(p, c); cs, sn = math.cos(a), math.sin(a); kv = dot(k, v); kx = cross(k, v)
        return [c[j] + v[j] * cs + kx[j] * sn + k[j] * kv * (1 - cs) for j in range(3)]
    low_row = height - 1
    for h in layout.get("fin_hinges", []):
        c = ch[h["channel"]]
        for deg in range(-90, 91, 5):
            a = math.radians(h["model_deg"] - deg)
            for i in range(c["start"], c["start"] + c["count"]):
                q = turned(leds[i], h["point"], norm(h["turn"]), a)
                low_row = max(low_row, round((v1 - q[2] + oy) / cell - 0.5))
    extra = low_row - (height - 1)
    grid += [[-1] * width for _ in range(extra)]
    height += extra
    assert height <= 255

    # The fin panel: big enough for both projections of the whole swing.
    finpanel = None
    if args.fin_panel_mm > 0 and layout.get("fin_hinges"):
        swing = []
        for h in layout["fin_hinges"]:
            c = ch[h["channel"]]
            for deg in range(-90, 91, 5):
                a = math.radians(h["model_deg"] - deg)
                swing += [turned(leds[i], h["point"], norm(h["turn"]), a) for i in range(c["start"], c["start"] + c["count"])]
        mm = args.fin_panel_mm
        ys, zs, xs = [q[1] for q in swing], [q[2] for q in swing], [q[0] for q in swing]
        pw = int((max(ys) - min(ys)) / mm) + 2
        ph = int(max(max(zs) - min(zs), max(xs) - min(xs)) / mm) + 2
        px, py = width, 0
        assert px + pw <= 255 and ph <= 255
        # each projection centred in the panel
        side_y0 = min(ys) - (pw * mm - (max(ys) - min(ys))) / 2
        side_z1 = max(zs) + (ph * mm - (max(zs) - min(zs))) / 2
        top_y0 = side_y0
        top_x0 = min(xs) - (ph * mm - (max(xs) - min(xs))) / 2
        finpanel = [px, py, pw, ph, mm, side_y0, side_z1, top_y0, top_x0]
        for row in grid:
            row += [-1] * pw
        if ph > height:
            grid += [[-1] * (width + pw) for _ in range(ph - height)]
            height = ph
        width += pw
        # The capability anchor (see the docstring): the LED whose own cell is last.
        last = max((r * width + c, led) for r, row in enumerate(grid) for c, led in enumerate(row) if led >= 0)
        assert last[0] > py * width + px
        grid[py][px] = last[1]

    # Fin geometry for following the real fin angle (see the module docstring).
    num = lambda x: ("%.3f" % x).rstrip("0").rstrip(".")
    fingeo = [num(cell), num(u0), num(v1), num(ox), num(oy), str(len(layout.get("fin_hinges", [])))]
    rest = []
    for h in layout.get("fin_hinges", []):
        c = ch[h["channel"]]
        fingeo += ["0" if h["side"] == "port" else "1", str(c["start"]), str(c["count"])]
        fingeo += [num(x) for x in h["point"]] + [num(x) for x in h["turn"]] + [num(h["model_deg"])]
        rest += [num(x) for i in range(c["start"], c["start"] + c["count"]) for x in leds[i]]
    fingeo += rest

    os.makedirs(OUT_DIR, exist_ok=True)
    out = os.path.join(OUT_DIR, "ledmap.json")
    with open(out, "w") as f:
        f.write('{"n":"Nibbles side (starboard view)","width":%d,"height":%d,\n' % (width, height))
        f.write('"copy":%s,\n' % json.dumps(copy).replace(" ", ""))
        f.write('"fingeo":[%s],\n' % ",".join(fingeo))
        if finpanel:
            f.write('"finpanel":[%s],\n' % ",".join(num(v) for v in finpanel))
        f.write('"map":[\n')
        f.write(",\n".join(",".join(str(c) for c in row) for row in grid))
        f.write("]}\n")
    json.load(open(out))  # check it parses

    # Preview: mapped LEDs solid by channel, with a line to their exact spot;
    # partner LEDs as outlines.
    colours = {out_body: "#e53935", in_body: "#1e88e5", port_fin: "#43a047", sb_fin: "#fb8c00"}
    chan = {k: n for n, c in ch.items() for k in range(c["start"], c["start"] + c["count"])}
    S = 10
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width * S}" height="{height * S + 20}" style="background:#fff">',
           f'<rect width="{width * S}" height="{height * S}" fill="#f4f4f4"/>']
    for gy in range(height):
        for gx in range(width):
            i = grid[gy][gx]
            if i < 0:
                continue
            if i in moved:
                x, y = pos[i]
                svg.append(f'<line x1="{(gx + .5) * S}" y1="{(gy + .5) * S}" x2="{(x + .5) * S:.1f}" y2="{(y + .5) * S:.1f}" stroke="#999" stroke-width="0.6"/>')
                svg.append(f'<rect x="{gx * S + 1}" y="{gy * S + 1}" width="{S - 2}" height="{S - 2}" fill="{colours[chan[i]]}"/>')
            else:
                svg.append(f'<rect x="{gx * S + 2}" y="{gy * S + 2}" width="{S - 4}" height="{S - 4}" fill="none" stroke="{colours[chan[i]]}"/>')
    svg.append(f'<text x="4" y="{height * S + 15}" font-size="12" font-family="sans-serif">{width}x{height} cells of {cell:.1f} mm; '
               f'solid: mapped (channel {out_body} red, starboard fin {sb_fin} orange), outline: copied partners</text></svg>')
    open(os.path.join(OUT_DIR, "ledmap_preview.svg"), "w").write("\n".join(svg))

    m = sorted(moved.values())
    pm = sorted(pmoved.values())
    print(f"{width} x {height} = {width * height} cells of {cell:.1f} mm ({extra} rows added below for the fins' swing); "
          f"~{width * height * 10 // 1024} KB of WLED RAM")
    print(f"mapped: {len(primary)} LEDs (channels {out_body} and {sb_fin}), moved off their exact spot: "
          f"median {m[len(m) // 2]:.2f}, 90% within {m[int(len(m) * .9)]:.2f}, max {m[-1]:.2f} cells")
    print(f"partners copied by the usermod: {len(pm)} LEDs (channels {in_body} and {port_fin}), "
          f"from their partner's cell: median {pm[len(pm) // 2]:.2f}, max {pm[-1]:.2f} cells")
    if finpanel:
        px, py, pw, ph = finpanel[:4]
        print(f"fin panel: {pw} x {ph} cells of {finpanel[4]:g} mm at column {px}; segments: body "
              f'{{"id":0,"start":0,"stop":{px},"startY":0,"stopY":{height}}}, fins '
              f'{{"id":1,"start":{px},"stop":{px + pw},"startY":{py},"stopY":{py + ph}}}')
    print(f"copy: {copy}   -> {os.path.relpath(out, REPO)} and ledmap_preview.svg")


if __name__ == "__main__":
    main()
