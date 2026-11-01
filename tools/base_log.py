#!/usr/bin/env python3
"""The base station's TF card log, over its USB serial port.

    python tools/base_log.py PORT time            set its clock from this computer's
    python tools/base_log.py PORT list            the files on the card
    python tools/base_log.py PORT get N [DIR]     fetch L0000N.CSV into DIR (default .)
    python tools/base_log.py PORT all [DIR]       fetch every file not already in DIR

Needs pyserial (the ESP-IDF Python environment has it).
"""
import base64
import collections
import os
import re
import sys
import time
import zlib

import serial


class Base:
    def __init__(self, port):
        self.p = serial.Serial(port, 115200, timeout=0.2, write_timeout=2)
        self.left = b""
        self.waiting = collections.deque()

    def line(self, until):
        """The next line, or None once it's past this time."""
        while not self.waiting:
            if time.time() >= until:
                return None
            self.left += self.p.read(8192)
            *full, self.left = self.left.split(b"\n")
            self.waiting.extend(x.strip().decode(errors="replace") for x in full)
        return self.waiting.popleft()

    def ask(self, command, reply, seconds=20):
        """Sends it until the board answers: opening the port may restart it."""
        end = time.time() + seconds
        while time.time() < end:
            self.p.write(command.encode() + b"\n")
            wait = time.time() + 2
            while (line := self.line(wait)) is not None:
                if line.startswith(reply):
                    return line
        sys.exit(f"no answer to {command!r}")


def listing(b):
    line = b.ask("log list", "LOGLIST")
    files = {}
    end = time.time() + 10
    while line != "LOGLIST END":
        m = re.fullmatch(r"LOGLIST (\S+) (-?\d+)", line or "")
        if m:
            files[m[1].upper()] = int(m[2])
        line = b.line(end)
        if line is None:
            sys.exit("the file list stopped short")
    return files


def fetch(b, number, folder):
    head = b.ask(f"log get {number}", "LOGFILE")
    m = re.fullmatch(r"LOGFILE (\S+) (\d+)", head)
    if not m:
        sys.exit(f"L{number:05d}: not on the card")
    name, size = m[1], int(m[2])
    data = bytearray()
    t0 = time.time()
    quiet = time.time() + 10     # gives up 10 s after the last piece of the file
    while (line := b.line(quiet)) is not None:
        if line.startswith("LOGD "):
            data += base64.b64decode(line[5:])
            quiet = time.time() + 10
        elif line.startswith("LOGFILE END"):
            crc = int(line.split()[2], 16)
            if len(data) != size or zlib.crc32(bytes(data)) != crc:
                sys.exit(f"{name}: damaged in transfer ({len(data)} of {size} bytes)")
            os.makedirs(folder, exist_ok=True)
            with open(os.path.join(folder, name), "wb") as f:
                f.write(data)
            print(f"{name}: {size} bytes in {time.time() - t0:.1f} s")
            return
    sys.exit(f"{name}: the transfer stopped at {len(data)} of {size} bytes")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    b = Base(sys.argv[1])
    what = sys.argv[2]
    if what == "time":
        print(b.ask(f"time {int(time.time())}", "TIME"))
    elif what == "list":
        for name, size in sorted(listing(b).items()):
            print(name, size)
    elif what == "get":
        fetch(b, int(sys.argv[3]), sys.argv[4] if len(sys.argv) > 4 else ".")
    elif what == "all":
        folder = sys.argv[3] if len(sys.argv) > 3 else "."
        for name in sorted(listing(b)):
            if not os.path.exists(os.path.join(folder, name)):
                fetch(b, int(name[1:6]), folder)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
