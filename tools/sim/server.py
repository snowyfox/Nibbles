#!/usr/bin/env python3
"""Nibbles lighting simulator bridge (Python standard library only).

    python3 tools/sim/server.py [--port 8080] [--wled 10.7.200.154] [--shark 10.7.200.253]

- Serves the simulator page from tools/sim/web.
- WebSocket /ws: the page sends 44-byte WLED audio sync packets, which go out
  over UDP to the simulator WLED (multicast 239.0.0.1:11988, the group WLED
  AudioReactive listens on, and straight to --wled).
- Listens for DDP on UDP 4048 (the simulator WLED's network LED bus), puts
  each frame back together and sends it to the page: one binary WebSocket
  message of RGB bytes per frame.
- Or, when the page switches to the live shark ({"source": "shark", "ip": ..}
  as a WebSocket text message), mirrors the shark's own WLED controller
  instead: asks its Nibbles usermod (every 2 s) to stream its LEDs as DDP to
  UDP 4050 and the eyes' audio features and telemetry as JSON to UDP 4051,
  and passes both to the page (the JSON as text messages). The page's sound
  then goes nowhere: the shark listens with its own mic.
- The base station's keypad: the page's simulated keys ({"base": {"key": ..,
  "down": ..}}) run through the base firmware's own Live / Standby logic
  (firmware/base/main/controls.c, built here with the computer's C compiler
  and called through ctypes); the page gets its state, key LED colours and
  the actions to carry out (preset and brightness changes, bumps).
- Or the real base, on USB ({"realbase": {"on": true}}): the page's keys go
  to its serial console ("key NAME down|up"), so it sends real commands and
  bumps over the radio; the page gets its key LED colours ("leds on") and a
  screenshot of its screen about once a second ("shot"). The port is found by
  the base's serial number (BASE_SERIAL), or --base PORT.
"""
import argparse
import asyncio
import re
import termios
import tty
import ctypes
import subprocess
import time
import base64
import hashlib
import json
import mimetypes
import os
import socket
import struct

WEB = os.path.join(os.path.dirname(os.path.abspath(__file__)), "web")
AUDIO_SYNC_GROUP = ("239.0.0.1", 11988)
DDP_PORT = 4048          # the simulator WLED's network LED bus
MIRROR_PORT = 4050       # the shark's mirror stream (DDP); its JSON state on MIRROR_PORT + 1
MIRROR_RENEW_S = 2

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
clients = set()
WEB_CONFIG = {}
mode = {"source": "sim", "ip": None}   # "sim": the bench WLED; "shark": mirror the real controller


SEND_ERRORS = (ConnectionError, RuntimeError, TimeoutError, OSError)
busy = set()  # pages still taking the previous frame: they skip frames rather than queue them


def drop(writer):
    clients.discard(writer)
    busy.discard(writer)
    try:
        writer.close()
    except Exception:
        pass


async def send_to(writer, data, opcode=0x2):
    if writer in busy:
        return
    busy.add(writer)
    try:
        await ws_send(writer, data, opcode)
    except SEND_ERRORS:
        drop(writer)
    finally:
        busy.discard(writer)


async def broadcast(data, opcode=0x2):
    for w in list(clients):
        await send_to(w, data, opcode)


def mode_message():
    return json.dumps({"source": mode["source"], "ip": mode["ip"]}).encode()


# ------------------------------------------------------------------ WebSocket

async def ws_send(writer, data, opcode=0x2):
    header = bytearray([0x80 | opcode])
    n = len(data)
    if n < 126:
        header.append(n)
    elif n < 65536:
        header += bytes([126]) + struct.pack(">H", n)
    else:
        header += bytes([127]) + struct.pack(">Q", n)
    if writer.is_closing():  # the page went away: don't write (asyncio warns on every write)
        raise ConnectionError("closed")
    writer.write(bytes(header) + data)
    await writer.drain()


async def ws_recv(reader):
    """One message from the browser: (opcode, payload); None when closed."""
    b0, b1 = await reader.readexactly(2)
    opcode, n = b0 & 0x0F, b1 & 0x7F
    if n == 126:
        n = struct.unpack(">H", await reader.readexactly(2))[0]
    elif n == 127:
        n = struct.unpack(">Q", await reader.readexactly(8))[0]
    mask = await reader.readexactly(4) if b1 & 0x80 else b"\0\0\0\0"
    data = bytearray(await reader.readexactly(n))
    for i in range(n):
        data[i] ^= mask[i % 4]
    if opcode == 0x8:
        return None
    return opcode, bytes(data)


# ------------------------------------------------------------------ HTTP

async def handle(reader, writer, audio_sock, wled_ip):
    try:
        request = await reader.readuntil(b"\r\n\r\n")
    except (asyncio.IncompleteReadError, asyncio.LimitOverrunError):
        writer.close()
        return
    lines = request.decode(errors="replace").split("\r\n")
    method, path, _ = (lines[0].split(" ") + ["", "", ""])[:3]
    headers = {}
    for line in lines[1:]:
        if ":" in line:
            k, v = line.split(":", 1)
            headers[k.strip().lower()] = v.strip()

    if path == "/ws" and headers.get("upgrade", "").lower() == "websocket":
        key = headers.get("sec-websocket-key", "")
        accept = base64.b64encode(hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode()).digest()).decode()
        writer.write(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                      f"Sec-WebSocket-Accept: {accept}\r\n\r\n").encode())
        await writer.drain()
        clients.add(writer)
        await ws_send(writer, mode_message(), 0x1)
        try:
            while True:
                msg = await ws_recv(reader)
                if msg is None:
                    break
                opcode, data = msg
                if opcode == 0x1:  # {"source": "sim"} or {"source": "shark", "ip": ".."}
                    try:
                        req = json.loads(data)
                    except ValueError:
                        continue
                    if req.get("source") in ("sim", "shark"):
                        await set_mode(req["source"], req.get("ip"))
                    elif "base" in req and base.lib:
                        await base.message(writer, req["base"])
                    elif "realbase" in req:
                        await realbase.message(writer, req["realbase"])
                elif opcode == 0x2 and len(data) == 44 and mode["source"] == "sim":  # audio sync packet
                    audio_sock.sendto(data, AUDIO_SYNC_GROUP)
                    if wled_ip:
                        audio_sock.sendto(data, (wled_ip, AUDIO_SYNC_GROUP[1]))
                elif opcode == 0x9:  # ping
                    await ws_send(writer, data, 0xA)
        except (asyncio.IncompleteReadError,) + SEND_ERRORS:
            pass
        finally:
            clients.discard(writer)
            writer.close()
        return

    # Settings for the page
    if path.split("?")[0] == "/config.json":
        body = json.dumps({**WEB_CONFIG, "bench": wled_ip}).encode()
        writer.write(f"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: {len(body)}\r\n"
                     "Cache-Control: no-cache\r\n\r\n".encode() + body)
        await writer.drain()
        writer.close()
        return

    # Static files
    rel = path.split("?")[0].lstrip("/") or "index.html"
    full = os.path.normpath(os.path.join(WEB, rel))
    if not full.startswith(WEB) or not os.path.isfile(full):
        writer.write(b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n")
    else:
        body = open(full, "rb").read()
        ctype = "application/wasm" if full.endswith(".wasm") else (mimetypes.guess_type(full)[0] or "application/octet-stream")
        writer.write(f"HTTP/1.1 200 OK\r\nContent-Type: {ctype}\r\nContent-Length: {len(body)}\r\n"
                     "Cache-Control: no-cache\r\n\r\n".encode() + body)
    await writer.drain()
    writer.close()


# ------------------------------------------------------------------ DDP

class DDPReceiver(asyncio.DatagramProtocol):
    """Collects DDP packets into whole frames (the push flag ends a frame);
    passes them on while the page shows this source."""

    def __init__(self, source):
        self.source = source
        self.frame = bytearray()
        self.frames = 0

    def datagram_received(self, data, addr):
        if len(data) < 10:
            return
        flags = data[0]
        offset, length = struct.unpack(">IH", data[4:10])
        payload = data[10:10 + length]
        end = offset + len(payload)
        if len(self.frame) < end:
            self.frame.extend(b"\0" * (end - len(self.frame)))
        self.frame[offset:end] = payload
        if flags & 0x01 and mode["source"] == self.source:  # push: the frame is complete
            self.frames += 1
            frame = bytes(self.frame)
            for w in list(clients):
                asyncio.ensure_future(send_to(w, frame))


class MirrorState(asyncio.DatagramProtocol):
    """The shark's JSON state (eyes' audio features and telemetry), to the page."""

    def datagram_received(self, data, addr):
        if mode["source"] == "shark":
            asyncio.ensure_future(broadcast(data, 0x1))


def request_mirror(ip, seconds):
    """Ask the shark's usermod to stream to us (seconds 0 stops it)."""
    import urllib.request
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    probe.connect((ip, 80))  # no packet is sent; finds our address on the shark's network
    me = probe.getsockname()[0]
    probe.close()
    body = json.dumps({"nibbles": {"mirror": {"ip": me, "port": MIRROR_PORT, "s": seconds}}}).encode()
    req = urllib.request.Request(f"http://{ip}/json/state", data=body, headers={"Content-Type": "application/json"})
    urllib.request.urlopen(req, timeout=2).read()


async def set_mode(source, ip):
    old = dict(mode)
    mode["source"], mode["ip"] = source, ip if source == "shark" else None
    if old["source"] == "shark" and old["ip"] and old["ip"] != mode["ip"]:
        try:
            await asyncio.get_running_loop().run_in_executor(None, request_mirror, old["ip"], 0)
        except OSError:
            pass
    print(f"source: {source}{' ' + ip if source == 'shark' else ''}")
    await broadcast(mode_message(), 0x1)


async def keep_mirroring():
    failed = False
    while True:
        if mode["source"] == "shark" and mode["ip"] and clients:  # only while a page is watching
            try:
                await asyncio.get_running_loop().run_in_executor(None, request_mirror, mode["ip"], MIRROR_RENEW_S * 3)
                if failed:
                    print(f"mirroring {mode['ip']} again")
                failed = False
            except OSError as e:
                if not failed:
                    print(f"can't reach the shark at {mode['ip']}: {e}")
                failed = True
        await asyncio.sleep(MIRROR_RENEW_S)


# ------------------------------------------------------------------ base station keypad

class Base:
    """The base firmware's keypad logic (controls.c), shared by every page."""
    SOURCES = [os.path.join(HERE, "src", "base_host.c"), os.path.join(REPO, "firmware", "base", "main", "controls.c")]

    def __init__(self):
        self.lib = None
        self.owner = None          # the page that last pressed a key: it carries out the actions
        self.last = None
        self.t0 = time.monotonic()
        self.out = (ctypes.c_int * 64)()
        self.names = []

    def build(self):
        """Compile (if a source changed) and load; None if there's no C compiler."""
        out = os.path.join(HERE, "build", "libbase.so")
        deps = self.SOURCES + [os.path.join(REPO, "firmware", "base", "main", "controls.h")]
        try:
            if not os.path.exists(out) or any(os.path.getmtime(d) > os.path.getmtime(out) for d in deps):
                os.makedirs(os.path.dirname(out), exist_ok=True)
                subprocess.run(["cc", "-shared", "-fPIC", "-O2", "-I", os.path.join(REPO, "firmware", "base", "main"),
                                "-I", os.path.join(REPO, "shared", "nibbles_link", "include"), "-o", out] + self.SOURCES,
                               check=True)
            lib = ctypes.CDLL(out)
        except (OSError, subprocess.CalledProcessError) as e:
            print(f"base keypad not available (no C compiler?): {e}")
            return
        lib.base_key_name.restype = ctypes.c_char_p
        lib.base_target_name.restype = ctypes.c_char_p
        lib.base_init(0)
        self.names = [lib.base_key_name(k).decode() for k in range(lib.base_key_count())]
        self.lib = lib

    def now_ms(self):
        return int((time.monotonic() - self.t0) * 1000) & 0xFFFFFFFF

    def actions(self, n):
        return [list(self.out[i * 4:i * 4 + 4]) for i in range(n)]

    def state(self):
        st = (ctypes.c_int * 13)()
        self.lib.base_state(st)
        leds = (ctypes.c_uint * len(self.names))()
        self.lib.base_leds(self.now_ms(), leds)
        v = list(st)
        return {"live": v[0:3], "standby": v[3:6], "target": self.lib.base_target_name(v[6]).decode(),
                "page": v[7], "locked": bool(v[8]), "preview": bool(v[9]), "latch": bool(v[10]), "diff": v[11],
                "bri": v[12], "leds": {self.names[k]: "#%06x" % leds[k] for k in range(1, len(self.names))}}

    async def send(self, writer, msg):
        await send_to(writer, json.dumps({"base": msg}).encode(), 0x1)

    async def message(self, writer, m):
        if "live" in m:            # what the page's WLED and eyes show: [body, eyes, fin preset, brightness, fin sensor]
            self.lib.base_set_live(*[int(x) for x in m["live"]])
        if "key" in m and m["key"] in self.names:
            self.owner = writer
            n = self.lib.base_key(self.names.index(m["key"]), 1 if m.get("down") else 0, self.now_ms(), self.out)
            await self.send(writer, {"actions": self.actions(n), **self.state()})
        elif "hello" in m:
            await self.send(writer, {"keys": self.names[1:], **self.state()})

    async def run(self):
        """Holding UNLOCK, auto-lock and the blinking TAKE: tick, and send changes to every page."""
        while True:
            await asyncio.sleep(0.05)
            if not self.lib or not clients:
                continue
            n = self.lib.base_tick(self.now_ms(), self.out)
            st = self.state()
            if n and self.owner in clients:
                await self.send(self.owner, {"actions": self.actions(n), **st})
            elif st != self.last:
                for w in list(clients):
                    await self.send(w, st)
            self.last = st


base = Base()


# ------------------------------------------------------------------ the real base, on USB

BASE_SERIAL = "28:84:85:91:27:08"   # the base prototype's USB serial number (its MAC)


def find_base_port():
    """The base's /dev/cu.usbmodem port, from its USB serial number and location
    (macOS names the port after the location: 0x00400000 -> usbmodem401)."""
    try:
        out = subprocess.run(["system_profiler", "SPUSBDataType"], capture_output=True, text=True, timeout=20).stdout
    except (OSError, subprocess.TimeoutExpired):
        return None
    i = out.find("Serial Number: " + BASE_SERIAL)
    if i < 0:
        return None
    m = re.search(r"Location ID: 0x([0-9a-f]+)", out[i:i + 400])
    if not m:
        return None
    port = "/dev/cu.usbmodem%x01" % (int(m.group(1), 16) >> 20)
    return port if os.path.exists(port) else None


class RealBase:
    """The base prototype's USB serial console. Opening the port may restart
    the base (the USB-JTAG reset); it is opened once, when a page first asks."""

    def __init__(self, port):
        self.port = port
        self.fd = None
        self.buf = b""
        self.shot = None          # base64 lines while a screenshot comes in
        self.shot_size = None
        self.shot_asked = 0.0     # when the last one was asked for
        self.shot_rle = False
        self.shot_soon = False    # a key changed something: the next one sooner
        self.on = set()           # pages driving the real base

    def open(self):
        if self.fd is not None:
            return True
        port = self.port or find_base_port()
        if not port:
            print("real base: not found on USB")
            return False
        try:
            fd = os.open(port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
            tty.setraw(fd)
            attrs = termios.tcgetattr(fd)
            attrs[2] &= ~termios.HUPCL          # don't drop the lines on close
            termios.tcsetattr(fd, termios.TCSANOW, attrs)
        except OSError as e:
            print(f"real base: can't open {port}: {e}")
            return False
        self.fd = fd
        asyncio.get_running_loop().add_reader(fd, self.readable)
        print(f"real base: {port}")
        return True

    def write(self, line):
        if self.fd is not None:
            try:
                os.write(self.fd, (line + "\n").encode())
            except OSError:
                pass

    def readable(self):
        try:
            self.buf += os.read(self.fd, 65536)
        except BlockingIOError:
            return
        except OSError:
            asyncio.get_running_loop().remove_reader(self.fd)
            os.close(self.fd)
            self.fd = None
            return
        *lines, self.buf = self.buf.split(b"\n")
        for raw in lines:
            line = raw.decode(errors="replace").strip()
            if line.startswith("LEDS "):
                asyncio.ensure_future(self.send({"leds": line[5:].split(",")}))
            elif line == "SHOT failed":
                self.shot = None
            elif line.startswith("SHOT ") and line != "SHOT END":
                parts = line.split()
                self.shot, self.shot_size = [], (int(parts[1]), int(parts[2]))
                self.shot_rle = len(parts) > 3 and parts[3] == "rle565le"
            elif line == "SHOT END" and self.shot is not None:
                w, h = self.shot_size
                data = "".join(self.shot)
                # whole (a line lost to a log line would shift the picture): the
                # page checks a run-length one by its pixel count
                if self.shot_rle or len(data) == (w * h * 2 + 2) // 3 * 4:
                    asyncio.ensure_future(self.send({"shot": data, "w": w, "h": h, "rle": self.shot_rle}))
                self.shot = None
            elif self.shot is not None and re.fullmatch(r"[A-Za-z0-9+/=]+", line):
                self.shot.append(line)    # log lines can land in between: only base64 is kept

    async def send(self, msg):
        for w in list(self.on):
            if w in clients:
                await send_to(w, json.dumps({"realbase": msg}).encode(), 0x1)
            else:
                self.on.discard(w)

    async def message(self, writer, m):
        if "on" in m:
            if m["on"] and self.open():
                self.on.add(writer)
                self.write("leds on")
            else:
                self.on.discard(writer)
            await send_to(writer, json.dumps({"realbase": {"connected": self.fd is not None and writer in self.on}}).encode(), 0x1)
        elif "key" in m and writer in self.on and re.fullmatch(r"[a-z0-9]+", str(m["key"])):
            self.write(f"key {m['key']} {'down' if m.get('down') else 'up'}")
            self.shot_soon = True

    async def run(self):
        """A screenshot (run-length encoded, so small) every second while a page
        is driving the base, and soon after each key; one that never finishes
        (cut short, or lost) is given up after 4 s."""
        while True:
            await asyncio.sleep(0.15)
            if self.fd is None or not self.on:
                continue
            since = time.monotonic() - self.shot_asked
            if self.shot is not None and since < 4:
                continue
            if since < (0.15 if self.shot_soon else 1.0):
                continue
            self.shot = None
            self.shot_soon = False
            self.shot_asked = time.monotonic()
            self.write("shot rle")


realbase = None


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--wled", default="10.7.200.154", help="simulator WLED address (also gets audio directly)")
    ap.add_argument("--shark", default="10.7.200.253", help="the shark's WLED controller, offered to the page for mirroring")
    ap.add_argument("--source", choices=["sim", "shark"], default="sim",
                    help="start mirroring the shark (--shark) instead of the bench WLED; the page can change it")
    ap.add_argument("--base", default=None, help="the real base's USB serial port (default: found by its serial number)")
    args = ap.parse_args()

    audio_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    audio_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    loop = asyncio.get_running_loop()
    await loop.create_datagram_endpoint(lambda: DDPReceiver("sim"), local_addr=("0.0.0.0", DDP_PORT))
    await loop.create_datagram_endpoint(lambda: DDPReceiver("shark"), local_addr=("0.0.0.0", MIRROR_PORT))
    await loop.create_datagram_endpoint(MirrorState, local_addr=("0.0.0.0", MIRROR_PORT + 1))
    if args.source == "shark":
        mode["source"], mode["ip"] = "shark", args.shark
    asyncio.ensure_future(keep_mirroring())
    base.build()
    asyncio.ensure_future(base.run())
    global realbase
    realbase = RealBase(args.base)
    asyncio.ensure_future(realbase.run())
    WEB_CONFIG["shark"] = args.shark
    server = await asyncio.start_server(lambda r, w: handle(r, w, audio_sock, args.wled), "0.0.0.0", args.port)
    print(f"Nibbles simulator: http://localhost:{args.port}/  (bench WLED {args.wled}, DDP on UDP {DDP_PORT}; "
          f"shark {args.shark}, mirror on UDP {MIRROR_PORT})")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
