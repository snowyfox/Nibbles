#!/usr/bin/env python3
"""Play the base station's keys from a MIDI keyboard (Python standard library only).

    python3 tools/midi_keys/server.py [--port 8090] [--base /dev/cu.usbmodemNNN]

then open http://localhost:8090/ in Chrome (Web MIDI) and allow MIDI.

- The page reads the MIDI keyboard (an M-Audio KeyStudio here) and sends each
  mapped key press and release here; this writes "key NAME down|up" to the
  real base's USB serial console, so the base runs the keys as if pressed.
- The base is found by its USB serial number (its MAC, BASE_SERIAL) through
  ioreg, and found again when it comes back after a restart or a replug.
  Opening the port can restart the base: give it ~15 s to find the radio.
- The key mapping lives in mapping.json next to this file (the page edits it).
  A trigger can also be a combination: a MIDI key pressed while base
  modifiers are held ("layer+note:64"), which the page resolves itself.
- A slider (the KeyStudio's volume slider, CC 7) sets WLED's brightness:
  "wled bri N" (0..255) to the base, which sends it to the Live brain (the
  Standby brain follows within a second).
- The page also shows the base's key LED colours ("leds on"), WLED's
  brightness from the base's status line, and the end of its log.
"""
import argparse
import json
import os
import re
import select
import subprocess
import termios
import threading
import time
import tty
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
WEB = os.path.join(HERE, "web")
MAPPING = os.path.join(HERE, "mapping.json")
DEFAULT_MAPPING = os.path.join(HERE, "mapping.default.json")
CONTROLS_C = os.path.join(REPO, "firmware", "base", "main", "controls.c")
CONTROLS_H = os.path.join(REPO, "firmware", "base", "main", "controls.h")

BASE_SERIAL = "80:45:6B:35:D0:14"   # the base's USB serial number (its MAC)
LOG_LINES = 60
ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")


def key_names():
    """The base's key names, in ctl_key_t order from KEY_NUM1 (read from the
    firmware, so a new key shows up here by itself)."""
    try:
        src = open(CONTROLS_C).read()
        m = re.search(r"key_names\[[A-Z_]*\]\s*=\s*\{(.*?)\};", src, re.S)
        names = re.findall(r'"([^"]+)"', m.group(1))
        if names and names[0] == "none":
            return names[1:]
    except (OSError, AttributeError):
        pass
    return ["1", "2", "3", "4", "5", "6", "7", "8", "all", "body", "eyes", "fins", "page",
            "latch", "blackout", "finsensor", "take", "flash", "unlock"]


def palette_names():
    """The palette keys' names (CTL_DEFAULT_PALETTE_NAMES in controls.h)."""
    try:
        src = open(CONTROLS_H).read()
        m = re.search(r"CTL_DEFAULT_PALETTE_NAMES\s*\{(.*?)\}", src)
        return re.findall(r'"([^"]+)"', m.group(1))
    except (OSError, AttributeError):
        return []


KEYS = key_names()
# Base keys a mapping can hold for a combination ("layer+note:64"): the base's
# modifiers, as hotkeys on a computer (controls.h).
MODIFIERS = [k for k in ("palette", "layer", "bright", "page") if k in KEYS]
PALETTES = palette_names()
SLIDERS = ["brightness"]


def find_base_port():
    """The base's /dev/cu.usbmodem port, from its USB serial number. macOS names
    the port after the USB location: 0x00131400 -> usbmodem131401."""
    try:
        out = subprocess.run(["ioreg", "-p", "IOUSB", "-l", "-w0"], capture_output=True, text=True, timeout=10).stdout
    except (OSError, subprocess.TimeoutExpired):
        return None
    # each device's properties sit between its "+-o" lines
    for block in out.split("+-o ")[1:]:
        if f'"USB Serial Number" = "{BASE_SERIAL}"' not in block:
            continue
        m = re.search(r'"locationID" = (\d+)', block)
        if not m:
            return None
        port = "/dev/cu.usbmodem%s01" % format(int(m.group(1)), "x").rstrip("0")
        return port if os.path.exists(port) else None
    return None


class Base:
    """The base's USB serial console, kept open (and reopened) by a thread."""

    def __init__(self, port=None):
        self.fixed_port = port
        self.port = None
        self.fd = None
        self.lock = threading.Lock()
        self.log = []
        self.leds = []
        self.leds_at = 0.0
        self.error = "not looked for yet"
        self.down = set()           # keys this bridge holds down on the base
        self.sent = 0
        self.wled_bri = None        # WLED's brightness, from the base's status line
        threading.Thread(target=self.run, daemon=True).start()

    def open(self):
        port = self.fixed_port or find_base_port()
        if not port:
            self.error = "not found on USB"
            return False
        try:
            fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            tty.setraw(fd)
            attrs = termios.tcgetattr(fd)
            attrs[2] &= ~termios.HUPCL          # don't drop the lines on close
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
        except OSError as e:
            self.error = f"can't open {port}: {e.strerror}"
            return False
        self.fd, self.port, self.error = fd, port, None
        self.note(f"[bridge] opened {port}")
        self.write("leds on")
        return True

    def close(self, why):
        if self.fd is not None:
            try:
                os.close(self.fd)
            except OSError:
                pass
        self.fd = None
        self.error = why
        self.leds = []
        self.down.clear()            # a restarted base holds nothing down
        self.note(f"[bridge] {why}")

    def note(self, line):
        self.log.append(line)
        del self.log[:-LOG_LINES]

    def write(self, line):
        with self.lock:
            if self.fd is None:
                return False
            try:
                os.write(self.fd, (line + "\n").encode())
                return True
            except OSError as e:
                self.close(f"write failed: {e.strerror}")
                return False

    def key(self, name, down):
        if name not in KEYS:
            return False
        ok = self.write(f"key {name} {'down' if down else 'up'}")
        if ok:
            self.sent += 1
            (self.down.add if down else self.down.discard)(name)
        return ok

    def brightness(self, value):
        value = max(0, min(255, int(value)))
        ok = self.write(f"wled bri {value}")
        if ok:
            self.sent += 1
        return ok

    def all_up(self):
        for name in list(self.down):
            self.key(name, False)

    def run(self):
        buf = b""
        last_leds_ask = 0.0
        while True:
            if self.fd is None:
                if not self.open():
                    time.sleep(2)
                    continue
                buf = b""
                last_leds_ask = time.monotonic()
            try:
                ready, _, _ = select.select([self.fd], [], [], 0.5)
                if ready:
                    data = os.read(self.fd, 65536)
                    if not data:
                        self.close("the port closed (base restarted or unplugged?)")
                        continue
                    buf += data
            except BlockingIOError:
                continue
            except (OSError, ValueError) as e:
                self.close(f"the port went away ({getattr(e, 'strerror', e)})")
                continue
            *lines, buf = buf.split(b"\n")
            for raw in lines:
                line = ANSI.sub("", raw.decode(errors="replace")).strip()
                if line.startswith("LEDS "):
                    self.leds = line[5:].split(",")
                    self.leds_at = time.monotonic()
                    continue
                m = re.search(r"wled: (on|off) bri (\d+)", line)
                if m:
                    self.wled_bri = int(m.group(2)) if m.group(1) == "on" else 0
                if line and not re.fullmatch(r"[A-Za-z0-9+/=]{40,}", line):
                    self.note(line)
            # "leds on" is forgotten when the base restarts without the port
            # going away: ask again while no colours have come
            now = time.monotonic()
            if not self.leds and now - last_leds_ask > 5:
                last_leds_ask = now
                self.write("leds on")


base = None


def read_mapping():
    for path in (MAPPING, DEFAULT_MAPPING):
        try:
            with open(path) as f:
                return json.load(f), path == DEFAULT_MAPPING
        except (OSError, ValueError):
            continue
    return {"buttons": {}}, True


def valid_trigger(t):
    """A MIDI key or pedal ("note:60", "cc:64"), or one pressed while base
    modifiers are held ("layer+note:60", "palette+layer+note:60")."""
    *mods, last = t.split("+")
    return (re.fullmatch(r"(note|cc):\d{1,3}", last) is not None
            and all(m in MODIFIERS for m in mods) and len(set(mods)) == len(mods))


def valid_mapping(m):
    if not isinstance(m, dict) or not isinstance(m.get("buttons"), dict):
        return False
    for name, triggers in m["buttons"].items():
        if name not in KEYS or not isinstance(triggers, list):
            return False
        if not all(isinstance(t, str) and valid_trigger(t) for t in triggers):
            return False
    sliders = m.get("sliders", {})
    if not isinstance(sliders, dict):
        return False
    for name, triggers in sliders.items():
        if name not in SLIDERS or not isinstance(triggers, list):
            return False
        if not all(isinstance(t, str) and re.fullmatch(r"cc:\d{1,3}", t) for t in triggers):
            return False
    return True


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        pass

    def reply(self, code, body, ctype="application/json"):
        data = body if isinstance(body, bytes) else json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def body(self):
        n = int(self.headers.get("Content-Length") or 0)
        try:
            return json.loads(self.rfile.read(n) or b"null")
        except ValueError:
            return None

    def do_GET(self):
        if self.path in ("/", "/index.html"):
            with open(os.path.join(WEB, "index.html"), "rb") as f:
                return self.reply(200, f.read(), "text/html; charset=utf-8")
        if self.path == "/api/status":
            return self.reply(200, {
                "base": {"connected": base.fd is not None, "port": base.port, "error": base.error,
                         "serial": BASE_SERIAL, "held": sorted(base.down), "sent": base.sent,
                         "wled_bri": base.wled_bri},
                "keys": KEYS,
                "modifiers": MODIFIERS,
                "palettes": PALETTES,
                "leds": base.leds,
                "log": base.log[-25:],
            })
        if self.path == "/api/mapping":
            m, is_default = read_mapping()
            return self.reply(200, {"mapping": m, "default": is_default})
        if self.path == "/api/mapping/default":
            with open(DEFAULT_MAPPING) as f:
                return self.reply(200, {"mapping": json.load(f)})
        self.reply(404, {"error": "not found"})

    def do_POST(self):
        if self.path == "/api/key":
            m = self.body() or {}
            ok = base.key(str(m.get("name")), bool(m.get("down")))
            return self.reply(200 if ok else 409, {"ok": ok})
        if self.path == "/api/brightness":
            m = self.body() or {}
            try:
                ok = base.brightness(m.get("value"))
            except (TypeError, ValueError):
                return self.reply(400, {"ok": False})
            return self.reply(200 if ok else 409, {"ok": ok})
        if self.path == "/api/console":   # one console command line (e.g. "survey"); the reply shows in the log
            m = self.body() or {}
            line = str(m.get("line", "")).strip()
            if not line or "\n" in line or "\r" in line:
                return self.reply(400, {"ok": False})
            return self.reply(200 if base.write(line) else 409, {"ok": True})
        if self.path == "/api/allup":
            base.all_up()
            return self.reply(200, {"ok": True})
        if self.path == "/api/mapping":
            m = self.body()
            if not valid_mapping(m):
                return self.reply(400, {"error": "bad mapping"})
            tmp = MAPPING + ".tmp"
            with open(tmp, "w") as f:
                json.dump(m, f, indent=2)
                f.write("\n")
            os.replace(tmp, MAPPING)
            return self.reply(200, {"ok": True})
        self.reply(404, {"error": "not found"})


def main():
    global base
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8090)
    ap.add_argument("--base", default=None, help="the base's USB serial port (default: found by its serial number)")
    args = ap.parse_args()
    base = Base(args.base)
    server = ThreadingHTTPServer(("127.0.0.1", args.port), Handler)
    print(f"MIDI keys bridge: http://localhost:{args.port}/ (open it in Chrome)")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        base.all_up()


if __name__ == "__main__":
    main()
