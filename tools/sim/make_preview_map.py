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
leds = json.load(open(os.path.join(HERE, "web", "layout.json")))["leds"]
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
