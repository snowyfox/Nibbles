#!/usr/bin/env python3
"""Screenshot of the base station's display over its USB serial port.

    python tools/base_shot.py /dev/cu.usbmodem1301 shot.png ["command" ...]

Optional commands (e.g. "wled next") are sent first. The base prints the next
frame as base64 RGB565 between "SHOT w h rgb565le" and "SHOT END"; this writes
it as a PNG. Needs pyserial (the ESP-IDF Python environment has it).
"""
import base64
import re
import struct
import sys
import time
import zlib

import serial


def save_png(data, w, h, path):
    rows = []
    for y in range(h):
        row = bytearray([0])
        for x in range(w):
            v = data[(y * w + x) * 2] | data[(y * w + x) * 2 + 1] << 8
            row += bytes([((v >> 11) & 31) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31])
        rows.append(bytes(row))

    def chunk(tag, body):
        return struct.pack('>I', len(body)) + tag + body + struct.pack('>I', zlib.crc32(tag + body) & 0xffffffff)

    with open(path, 'wb') as f:
        f.write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) +
                chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b''))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    port, path, commands = sys.argv[1], sys.argv[2], sys.argv[3:]
    s = serial.Serial(port, 115200, timeout=0.1)
    s.read(1 << 20)
    for cmd in commands:
        s.write(cmd.encode() + b'\n')
        time.sleep(0.3)
    s.write(b'shot\n')
    buf, t0 = b'', time.time()
    while time.time() - t0 < 20 and b'SHOT END' not in buf:
        buf += s.read(65536)
    text = buf.decode(errors='replace')
    start, end = text.find('SHOT '), text.find('SHOT END')
    if start < 0 or end < 0 or text[start:].startswith('SHOT failed'):
        sys.exit('no screenshot received')
    w, h = map(int, text[start:].splitlines()[0].split()[1:3])
    lines = text[start:end].splitlines()[1:]
    data = base64.b64decode(''.join(l.strip() for l in lines if re.fullmatch(r'[A-Za-z0-9+/=]+', l.strip())))
    if len(data) != w * h * 2:
        sys.exit(f'got {len(data)} bytes, expected {w * h * 2}')
    save_png(data, w, h, path)
    print(f'{path}: {w}x{h}')


if __name__ == '__main__':
    main()
