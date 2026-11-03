#!/usr/bin/env python3
"""Update the base (or a mirror board) without a USB flash: firmware/base/main/ota.h.

    python tools/base_ota.py usb PORT [BIN]       over the board's USB serial console
    python tools/base_ota.py wifi HOST [BIN]      to a base on Wi-Fi ("ota wifi on", or
                                                  PAGE held 5 s with the keys locked)
    python tools/base_ota.py status PORT          which slot and build it runs

BIN defaults to firmware/base/build/nibbles_base.bin. The board writes the image
into its other slot, restarts into it, and keeps it once it has run for 30 s;
if it restarts before then, it goes back to the old one. A base with the mirror
cable on passes the new image to its mirror by itself.

USB mode is for testing the protocol the mirror cable uses (and for a board
whose USB is reachable but whose BOOT key isn't). Needs pyserial (the ESP-IDF
Python environment has it).
"""
import base64
import hashlib
import os
import sys
import time
import urllib.request
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BIN = os.path.join(HERE, "..", "firmware", "base", "build", "nibbles_base.bin")
CHUNK = 768   # OTA_CHUNK


def build_id(bin_path):
    """The build id the board reports: the ELF's SHA-256, first 16 hex digits."""
    elf = os.path.splitext(bin_path)[0] + ".elf"
    if not os.path.exists(elf):
        return None
    return hashlib.sha256(open(elf, "rb").read()).hexdigest()[:16]


class Board:
    def __init__(self, port):
        import serial
        self.p = serial.Serial(port, 115200, timeout=0.05, write_timeout=5)
        self.left = b""

    def line(self, until):
        while time.time() < until:
            if b"\n" in self.left:
                raw, self.left = self.left.split(b"\n", 1)
                return raw.strip().decode(errors="replace")
            self.left += self.p.read(4096)
        return None

    def ask(self, command, want, seconds):
        self.p.write(command.encode() + b"\n")
        end = time.time() + seconds
        while (line := self.line(end)) is not None:
            if line.startswith(want) or line.startswith("OTA ERR"):
                return line
        return None


def status(port):
    b = Board(port)
    for _ in range(10):   # opening the port may restart it
        line = b.ask("ota status", "OTA ", 2)
        if line:
            return line
    sys.exit("no answer to ota status")


def usb(port, bin_path):
    data = open(bin_path, "rb").read()
    print(f"{len(data)} bytes, build {build_id(bin_path) or '?'}")
    b = Board(port)
    for _ in range(10):   # opening the port may restart it
        if (r := b.ask("ota status", "OTA ", 2)):
            print(r)
            break
    else:
        sys.exit("no answer to ota status")
    r = b.ask(f"ota begin {len(data)}", "OTA READY", 60)
    if not r or not r.startswith("OTA READY"):
        sys.exit(f"begin: {r}")
    print(r)
    t0 = time.time()
    off = 0
    while off < len(data):
        chunk = data[off:off + CHUNK]
        line = f"ota data {off} {zlib.crc32(chunk):08x} {base64.b64encode(chunk).decode()}"
        for _ in range(5):
            r = b.ask(line, "OTA OK", 5)
            if r == f"OTA OK {off + len(chunk)}":
                break
            print(f"\n  retry at {off}: {r}")
        else:
            b.ask("ota abort", "OTA ABORTED", 5)
            sys.exit("gave up")
        off += len(chunk)
        print(f"\r  {off * 100 // len(data)}%", end="", flush=True)
    print(f"\r  sent in {time.time() - t0:.1f} s")
    r = b.ask("ota end", "OTA DONE", 30)
    print(r)
    if r != "OTA DONE":
        sys.exit(1)


def wifi(host, bin_path):
    data = open(bin_path, "rb").read()
    print(f"{len(data)} bytes, build {build_id(bin_path) or '?'} to http://{host}/ota")
    req = urllib.request.Request(f"http://{host}/ota", data=data, method="POST",
                                 headers={"Content-Type": "application/octet-stream"})
    t0 = time.time()
    with urllib.request.urlopen(req, timeout=120) as r:
        print(r.read().decode().strip(), f"({time.time() - t0:.1f} s)")


def main():
    if len(sys.argv) < 3 or sys.argv[1] not in ("usb", "wifi", "status"):
        sys.exit(__doc__)
    mode, where = sys.argv[1], sys.argv[2]
    bin_path = sys.argv[3] if len(sys.argv) > 3 else DEFAULT_BIN
    if mode == "status":
        print(status(where))
    elif mode == "usb":
        usb(where, bin_path)
    else:
        wifi(where, bin_path)


if __name__ == "__main__":
    main()
