#!/usr/bin/env python3
"""Push firmware to the shark's boards over the air, unattended, for when the
computer has no internet while it is on the Nibbles network (a hotel room on
Wi-Fi only). It joins Nibbles, updates each group in turn, checks each board
kept its new image, sends everything off Wi-Fi and rejoins the usual network,
whatever happens. Nobody needs to watch: read the log afterwards.

    python3 tools/ota_push.py [--detach] [--log FILE] [--password P] [--minutes N] GROUP...

GROUP: eyes, monitors, base (in that order whatever the order given; the base
last, as it relays everything else). Images: the usual build outputs
(firmware/eyes/build/nibbles.bin, firmware/monitor/build/nibbles_monitor.bin,
firmware/base/build-191/nibbles_base.bin); a board already on that build is
left alone. --detach starts it in the background (it survives the terminal,
or Claude losing its connection) and prints where the log is; the log's last
line starts "RESULT". --minutes: give up after this long (default 20); the
boards leave Wi-Fi by themselves after 10 minutes anyway.

"Kept" means the board reported the new build at least 35 s after it
restarted: an image that fails is rolled back by the board itself (30 s, the
eyes' frame check up to 2 min), and then it would report the old build. The
eyes also wait for the starboard eye to report the port eye on the same build.

Needs a brain on (hosting Nibbles) and the base on; no USB. Standard library,
plus tools/nibbles_wifi.sh (macOS) to change networks.
"""
import datetime
import os
import re
import subprocess
import sys
import time
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from base_ask import ask as brain_ask      # noqa: E402
from base_ota import build_id              # noqa: E402

BRAIN = "4.3.2.1"
BINS = {
    "eyes": os.path.join(ROOT, "firmware", "eyes", "build", "nibbles.bin"),
    "monitors": os.path.join(ROOT, "firmware", "monitor", "build", "nibbles_monitor.bin"),
    "base": os.path.join(ROOT, "firmware", "base", "build-191", "nibbles_base.bin"),
}
ORDER = ["eyes", "monitors", "base"]
MONITORS = ("Live monitor", "Standby monitor")
KEEP_S = 35               # a build still reported this long after the restart was kept

# "OTA Live monitor   http://4.3.2.4/ build 65a2faf096bebe8b, port eye: same build  (28:84:85:8D:40:4C)"
LIST_RE = re.compile(r"^OTA (.+?)\s+(http://([\d.]+)/|joining Wi-Fi|Wi-Fi off) build ([0-9a-f]{16})(.*?)\s+\(([0-9A-F:]+)\)\s*$")
# "OTA base Update: http://4.3.2.8/, build a869aed3a3074411" / "OTA base Wi-Fi off, build ..."
BASE_RE = re.compile(r"^OTA base (.*?), build ([0-9a-f]{16})")

deadline = 0.0


def log(msg):
    print(f"{datetime.datetime.now():%H:%M:%S} {msg}", flush=True)


class GiveUp(Exception):
    pass


def check_time():
    if time.time() > deadline:
        raise GiveUp("out of time")


def ask(line, tries=3):
    """A console line to the base through the brain; None if nothing came back."""
    for _ in range(tries):
        check_time()
        try:
            a = brain_ask(BRAIN, line, tries=1)
        except Exception as e:   # the brain not reachable for a moment
            a = None
            log(f"  ({line}: {e})")
        if a:
            return a
        time.sleep(2)
    return None


def boards():
    """The boards on (or just off) Wi-Fi, by name: {name: (ip or None, build, note)}."""
    out = {}
    for l in (ask("ota remote") or "").splitlines():
        m = LIST_RE.match(l.strip())
        if m:
            out[m.group(1).strip()] = (m.group(3), m.group(4), m.group(5).strip(", ").strip())
    return out


def base_state():
    m = BASE_RE.match((ask("ota wifi") or "").strip())
    if not m:
        return None, None
    ip = re.search(r"http://([\d.]+)/", m.group(1))
    return (ip.group(1) if ip else None), m.group(2)


def upload(ip, bin_path):
    data = open(bin_path, "rb").read()
    for attempt in range(2):
        check_time()
        try:
            req = urllib.request.Request(f"http://{ip}/ota", data=data, method="POST",
                                         headers={"Content-Type": "application/octet-stream"})
            t0 = time.time()
            with urllib.request.urlopen(req, timeout=120) as r:
                log(f"  {ip}: {r.read().decode().strip()} ({time.time() - t0:.1f} s)")
                return True
        except Exception as e:
            log(f"  {ip}: upload failed ({e}){', again' if attempt == 0 else ''}")
            time.sleep(5)
    return False


def wait_for(names, group, want, seconds, label):
    """Ask the group onto Wi-Fi until every name in names shows want(info); returns the last list."""
    end = time.time() + seconds
    next_on = 0
    got = {}
    while time.time() < end:
        check_time()
        if time.time() >= next_on:
            ask(f"ota remote on {group} 10")
            next_on = time.time() + 15
        time.sleep(3)
        got = boards()
        if all(n in got and want(got[n]) for n in names):
            return got
    missing = [n for n in names if not (n in got and want(got[n]))]
    log(f"  {label}: not seen for {', '.join(missing)} after {seconds} s")
    return got


def push_eyes(results):
    target = build_id(BINS["eyes"])
    log(f"eyes: target build {target}")
    got = wait_for(["Eyes"], "eyes", lambda b: b[0], 90, "Eyes on Wi-Fi")
    if "Eyes" not in got or not got["Eyes"][0]:
        results["eyes"] = "FAILED: the starboard eye never came onto Wi-Fi"
        return
    ip, build, note = got["Eyes"]
    if build == target:
        log("  the starboard eye already runs it")
    else:
        if not upload(ip, BINS["eyes"]):
            results["eyes"] = "FAILED: upload"
            ask("ota remote off eyes")
            return
        time.sleep(KEEP_S)
    # Kept, and the port eye updated over the cable (the starboard eye says "same build").
    got = wait_for(["Eyes"], "eyes", lambda b: b[1] == target and "same build" in b[2], 300, "port eye on the same build")
    e = got.get("Eyes")
    ask("ota remote off eyes")
    if not e or e[1] != target:
        results["eyes"] = f"FAILED: starboard eye reports {e[1] if e else 'nothing'} (rolled back?)"
    elif "same build" not in e[2]:
        results["eyes"] = f"PARTLY: starboard eye kept {target}; port eye: {e[2] or 'no word'}"
    else:
        results["eyes"] = f"ok: both eyes on {target}"


def push_monitors(results):
    target = build_id(BINS["monitors"])
    log(f"monitors: target build {target}")
    got = wait_for(MONITORS, "monitors", lambda b: b[0], 90, "monitors on Wi-Fi")
    sent = {}
    for name in MONITORS:
        if name not in got or not got[name][0]:
            results[name] = "FAILED: never came onto Wi-Fi"
            continue
        ip, build, _ = got[name]
        if build == target:
            log(f"  {name} already runs it")
            results[name] = f"ok: already on {target}"
            continue
        log(f"  {name} at {ip}")
        if upload(ip, BINS["monitors"]):
            sent[name] = time.time()
        else:
            results[name] = "FAILED: upload"
    if sent:
        time.sleep(max(0, KEEP_S - (time.time() - min(sent.values()))))
        got = wait_for(list(sent), "monitors", lambda b: b[1] == target, 180, "monitors on the new build")
        for name in sent:
            b = got.get(name)
            results[name] = f"ok: kept {target}" if b and b[1] == target else \
                f"FAILED: reports {b[1] if b else 'nothing'} (rolled back?)"
    ask("ota remote off monitors")


def push_base(results):
    target = build_id(BINS["base"])
    log(f"base: target build {target}")
    ip, build = base_state()
    if build == target:
        results["base"] = f"ok: already on {target}"
        return
    ask("ota wifi on")
    end = time.time() + 90
    while time.time() < end and not ip:
        time.sleep(3)
        ip, build = base_state()
    if not ip:
        results["base"] = "FAILED: never came onto Wi-Fi"
        ask("ota wifi off")
        return
    log(f"  base at {ip}")
    if not upload(ip, BINS["base"]):
        results["base"] = "FAILED: upload"
        ask("ota wifi off")
        return
    time.sleep(KEEP_S + 5)
    end = time.time() + 120
    while time.time() < end:
        _, build = base_state()
        if build == target:
            break
        time.sleep(5)
    results["base"] = f"ok: kept {target}" if build == target else f"FAILED: reports {build} (rolled back?)"


def wifi(cmd, *args):
    r = subprocess.run([os.path.join(HERE, "nibbles_wifi.sh"), cmd, *args], capture_output=True, text=True, timeout=120)
    out = (r.stdout + r.stderr).strip()
    log(f"  {out}")
    return r.returncode == 0


def run(groups, password, minutes):
    global deadline
    deadline = time.time() + minutes * 60
    results = {}
    for g in groups:
        if not os.path.exists(BINS[g]) or not build_id(BINS[g]):
            sys.exit(f"no image (or its .elf) for {g}: {BINS[g]}; build it first")
        log(f"{g}: {BINS[g]} build {build_id(BINS[g])}")
    log("joining Nibbles (no internet from here until the end)")
    try:
        for attempt in range(3):
            if wifi("join", password):
                break
            time.sleep(10)
        else:
            raise GiveUp("could not join Nibbles")
        time.sleep(3)
        if not ask("ota wifi", tries=5):
            raise GiveUp(f"the base doesn't answer through the brain at {BRAIN}")
        for g in groups:
            try:
                {"eyes": push_eyes, "monitors": push_monitors, "base": push_base}[g](results)
            except GiveUp:
                raise
            except Exception as e:
                results[g] = f"FAILED: {e}"
                log(f"  {g}: {e}")
    except GiveUp as e:
        log(f"giving up: {e}")
        results["run"] = f"gave up: {e}"
    finally:
        global_deadline_extra = time.time() + 60   # the clean-up always runs, even out of time
        deadline = global_deadline_extra
        try:
            ask("ota remote off all", tries=2)
            ask("ota wifi off", tries=2)
        except GiveUp:
            pass
        log("leaving Nibbles")
        wifi("leave")
    for k, v in results.items():
        log(f"  {k}: {v}")
    bad = [k for k, v in results.items() if not v.startswith("ok")]
    log(f"RESULT {'all ok' if not bad else 'problems: ' + ', '.join(bad)}")
    return 0 if not bad else 1


def main():
    args = sys.argv[1:]
    opts = {"--log": None, "--password": "wled1234", "--minutes": "20"}
    detach = "--detach" in args
    if detach:
        args.remove("--detach")
    for k in list(opts):
        if k in args:
            i = args.index(k)
            opts[k] = args[i + 1]
            del args[i:i + 2]
    groups = [g for g in ORDER if g in args]
    if not groups or any(a not in ORDER for a in args):
        sys.exit(__doc__)
    if detach:
        path = opts["--log"] or os.path.join(ROOT, "logs", f"ota_push_{datetime.datetime.now():%Y%m%d_%H%M%S}.log")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        cmd = [sys.executable, os.path.abspath(__file__), "--password", opts["--password"],
               "--minutes", opts["--minutes"], *groups]
        with open(path, "w") as f:
            p = subprocess.Popen(cmd, stdout=f, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                 start_new_session=True, cwd=ROOT)
        print(f"started (pid {p.pid}); log: {path}")
        return 0
    return run(groups, opts["--password"], float(opts["--minutes"]))


if __name__ == "__main__":
    sys.exit(main())
