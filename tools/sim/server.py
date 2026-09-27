#!/usr/bin/env python3
"""Nibbles lighting simulator bridge (Python standard library only).

    python3 tools/sim/server.py [--port 8080] [--wled 10.7.200.226] [--shark 10.7.200.253]

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
"""
import argparse
import asyncio
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

clients = set()
WEB_CONFIG = {}
mode = {"source": "sim", "ip": None}   # "sim": the bench WLED; "shark": mirror the real controller


async def broadcast(data, opcode=0x2):
    for w in list(clients):
        try:
            await ws_send(w, data, opcode)
        except (ConnectionError, RuntimeError):
            clients.discard(w)


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
                elif opcode == 0x2 and len(data) == 44 and mode["source"] == "sim":  # audio sync packet
                    audio_sock.sendto(data, AUDIO_SYNC_GROUP)
                    if wled_ip:
                        audio_sock.sendto(data, (wled_ip, AUDIO_SYNC_GROUP[1]))
                elif opcode == 0x9:  # ping
                    await ws_send(writer, data, 0xA)
        except (asyncio.IncompleteReadError, ConnectionError):
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
                asyncio.ensure_future(self.send(w, frame))

    async def send(self, writer, frame):
        try:
            await ws_send(writer, frame)
        except (ConnectionError, RuntimeError):
            clients.discard(writer)


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


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--wled", default="10.7.200.226", help="simulator WLED address (also gets audio directly)")
    ap.add_argument("--shark", default="10.7.200.253", help="the shark's WLED controller, offered to the page for mirroring")
    args = ap.parse_args()

    audio_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    audio_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    loop = asyncio.get_running_loop()
    await loop.create_datagram_endpoint(lambda: DDPReceiver("sim"), local_addr=("0.0.0.0", DDP_PORT))
    await loop.create_datagram_endpoint(lambda: DDPReceiver("shark"), local_addr=("0.0.0.0", MIRROR_PORT))
    await loop.create_datagram_endpoint(MirrorState, local_addr=("0.0.0.0", MIRROR_PORT + 1))
    asyncio.ensure_future(keep_mirroring())
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
