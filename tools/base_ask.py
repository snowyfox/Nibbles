#!/usr/bin/env python3
"""Send a console line to the base over the radio, through a brain: no USB.

    python3 tools/base_ask.py "ota remote on eyes"      [--brain 4.3.2.1]
    python3 tools/base_ask.py "ota remote"
    python3 tools/base_ask.py "ota wifi on"   /   "ota wifi"   /   "ota wifi off"

The computer must be on the Nibbles network (tools/nibbles_wifi.sh join). The
brain hosting it (the Right Brain, 4.3.2.1) passes the line to the base with
NL_MSG_CONSOLE (its /keys/ask and /keys/answer); the base answers keymap, keys,
"ota remote ..." and "ota wifi ..." this way (firmware/base/main/ota_peers.h).
Standard library only.
"""
import json
import sys
import time
import urllib.parse
import urllib.request


def ask(brain, line, tries=2):
    for _ in range(tries):
        q = urllib.parse.urlencode({"c": line})
        with urllib.request.urlopen(f"http://{brain}/keys/ask?{q}", timeout=5) as r:
            rid = json.load(r)["id"]
        until = time.time() + 6
        while time.time() < until:
            try:
                with urllib.request.urlopen(f"http://{brain}/keys/answer?id={rid}", timeout=3) as r:
                    if r.status == 200:
                        return r.read().decode(errors="replace").strip()
            except urllib.error.HTTPError:
                pass
            time.sleep(0.3)
    return None   # the base may have been off its radio channel (joining Wi-Fi): ask again


def main():
    args = sys.argv[1:]
    brain = "4.3.2.1"
    if "--brain" in args:
        i = args.index("--brain")
        brain = args[i + 1]
        del args[i:i + 2]
    if len(args) != 1:
        print(__doc__)
        sys.exit(2)
    answer = ask(brain, args[0])
    if answer is None:
        print("no answer from the base (is it on, and the brain on the radio?)")
        sys.exit(1)
    print(answer)


if __name__ == "__main__":
    main()
