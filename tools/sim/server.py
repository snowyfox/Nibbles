#!/usr/bin/env python3
"""Nibbles lighting simulator bridge (Python standard library only).

    python3 tools/sim/server.py [--port 8080] [--wled 10.7.200.226]

- Serves the simulator page from tools/sim/web.
- WebSocket /ws: the page sends 44-byte WLED audio sync packets, which go out
  over UDP to the simulator WLED (multicast 239.0.0.1:11988, the group WLED
  AudioReactive listens on, and straight to --wled).
- Listens for DDP on UDP 4048 (the simulator WLED's network LED bus), puts
  each frame back together and sends it to the page: one binary WebSocket
  message of RGB bytes per frame.
"""
import argparse
import asyncio
import base64
import hashlib
import mimetypes
import os
import socket
import struct

WEB = os.path.join(os.path.dirname(os.path.abspath(__file__)), "web")
AUDIO_SYNC_GROUP = ("239.0.0.1", 11988)
DDP_PORT = 4048

clients = set()


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
        try:
            while True:
                msg = await ws_recv(reader)
                if msg is None:
                    break
                opcode, data = msg
                if opcode == 0x2 and len(data) == 44:  # audio sync packet
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
    """Collects DDP packets into whole frames (the push flag ends a frame)."""

    def __init__(self):
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
        if flags & 0x01:  # push: the frame is complete
            self.frames += 1
            frame = bytes(self.frame)
            for w in list(clients):
                asyncio.ensure_future(self.send(w, frame))

    async def send(self, writer, frame):
        try:
            await ws_send(writer, frame)
        except (ConnectionError, RuntimeError):
            clients.discard(writer)


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--wled", default="10.7.200.226", help="simulator WLED address (also gets audio directly)")
    args = ap.parse_args()

    audio_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    audio_sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, 2)
    loop = asyncio.get_running_loop()
    ddp = DDPReceiver()
    await loop.create_datagram_endpoint(lambda: ddp, local_addr=("0.0.0.0", DDP_PORT))
    server = await asyncio.start_server(lambda r, w: handle(r, w, audio_sock, args.wled), "0.0.0.0", args.port)
    print(f"Nibbles simulator: http://localhost:{args.port}/  (WLED {args.wled}, DDP on UDP {DDP_PORT})")
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
