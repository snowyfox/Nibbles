"""Bench test: overlapping key bumps must leave the brains as they were.

Plays key sequences through the MIDI keys bridge (tools/midi_keys/server.py,
POST /api/key on port 8090), reads each brain's WLED state over its USB
serial port ({"v":true}), and compares the state before and after each
sequence (segments and slots that are off only count as off).

    source ~/.espressif/tools/activate_idf_v5.5.5.sh   # for pyserial
    python3 tools/bump_overlap_test.py [all|flash|probe] [SEED] [RANDOM_RUNS]

Edit BRAINS for the ports (ls /dev/cu.usbmodem*; check serial numbers).
Opening a brain's USB serial port restarts it: the script waits 10 s, then
does one bump so the Live brain shows its look, and keeps the ports open.
Shark-side USB: follow the ground rule (the shark off the power board).
"""
import serial, sys, time, json, random, urllib.request
BRAINS = {"A": "/dev/cu.usbmodem1312101", "B": "/dev/cu.usbmodem1311101"}
ports = {}
for k, p in BRAINS.items():
    s = serial.Serial(); s.port = p; s.baudrate = 115200; s.timeout = 0.1; s.dtr = False; s.rts = False; s.open(); ports[k] = s
time.sleep(0.5)

def snap(k):
    s = ports[k]; s.reset_input_buffer(); s.write(b'{"v":true}\n')
    t = time.time(); buf = b''
    while time.time() - t < 1.5:
        buf += s.read(16384)
        if b'"info"' in buf: break
    txt = buf.decode('utf-8', 'replace'); i = txt.find('{"on"')
    j = txt.find(',"info"', i)
    d = json.JSONDecoder().raw_decode(txt[i:])[0]
    for x in ("transition", "udpn", "nl", "lor", "AudioReactive"): d.pop(x, None)
    for sg in d.get("seg", []): sg.pop("len", None)
    d["seg"] = [sg if sg.get("on") else {"id": sg["id"], "on": False} for sg in d.get("seg", [])]
    d["nibbles"]["slots"] = [x if x.get("on") else {"id": x["id"], "on": False} for x in d["nibbles"]["slots"]]
    d["seg"] = [sg for sg in d["seg"] if sg.get("on")]; d["nibbles"]["slots"] = [x for x in d["nibbles"]["slots"] if x.get("on")]
    return d

def key(name, down):
    req = urllib.request.Request("http://localhost:8090/api/key", data=json.dumps({"name": name, "down": down}).encode(),
                                 headers={"Content-Type": "application/json"})
    urllib.request.urlopen(req).read()

def diff(a, b, path=""):
    out = []
    if isinstance(a, dict) and isinstance(b, dict):
        for k in set(a) | set(b):
            out += diff(a.get(k), b.get(k), f"{path}.{k}")
    elif isinstance(a, list) and isinstance(b, list) and len(a) == len(b):
        for i, (x, y) in enumerate(zip(a, b)): out += diff(x, y, f"{path}[{i}]")
    elif a != b:
        out.append(f"{path}: {a} -> {b}")
    return out

def play(seq):
    for name, down, wait in seq:
        key(name, down); time.sleep(wait)

def run(label, seq, settle=3.0):
    before = {k: snap(k) for k in ports}
    play(seq); time.sleep(settle)
    after = {k: snap(k) for k in ports}
    bad = {k: diff(before[k], after[k]) for k in ports}
    ok = not any(bad.values())
    print(f"{'PASS' if ok else 'FAIL'} {label}")
    for k, d in bad.items():
        for l in d[:12]: print(f"   {k} {l}")
    sys.stdout.flush()
    return ok

def overlap(a, b, gap):
    return [(a, True, gap), (b, True, gap), (a, False, gap), (b, False, gap)]

def rand_seq(pool, n, rng):
    seq, held = [], []
    for _ in range(n):
        if held and (len(held) >= 2 or rng.random() < 0.45):
            k = held.pop(rng.randrange(len(held))); seq.append((k, False, rng.uniform(0.02, 0.35)))
        else:
            k = rng.choice([x for x in pool if x not in held]); held.append(k); seq.append((k, True, rng.uniform(0.02, 0.35)))
    for k in held: seq.append((k, False, rng.uniform(0.02, 0.2)))
    return seq

time.sleep(10); play([("1", True, 0.6), ("1", False, 3.0)])
for k in ports: d = snap(k); print("start", k, d["ps"], [(x["id"], x.get("n"), x["on"], x["col"][0], x["fx"]) for x in d["seg"]])
mode = sys.argv[1] if len(sys.argv) > 1 else "all"
if mode == "flash":
    for i in range(12):
        run(f"flash alone {i}", [("flash", True, 0.3), ("flash", False, 0.05)])
        run(f"blackout alone {i}", [("blackout", True, 0.3), ("blackout", False, 0.05)])
        run(f"flash/blackout overlap {i}", overlap("flash", "blackout", 0.15))
        run(f"blackout/flash overlap {i}", overlap("blackout", "flash", 0.15))
    sys.exit()
if mode == "probe":
    print(json.dumps(snap("A"))[:1500]); key("1", True); time.sleep(1.0); print(json.dumps(snap("A"))[:1500]); key("1", False); time.sleep(2); print(diff(snap("A"), snap("A")))
    sys.exit()
res = []
for gap in (0.25, 0.1, 0.04):
    res.append(run(f"numbers 1/2 overlap gap {gap}", overlap("1", "2", gap)))
    res.append(run(f"numbers 1/2/3 chain gap {gap}", [("1",True,gap),("2",True,gap),("1",False,gap),("3",True,gap),("2",False,gap),("3",False,gap)]))
    res.append(run(f"palette1/palette2 overlap gap {gap}", overlap("palette1", "palette2", gap)))
    res.append(run(f"number 1 then palette3 gap {gap}", overlap("1", "palette3", gap)))
    res.append(run(f"palette3 then number 2 gap {gap}", overlap("palette3", "2", gap)))
    res.append(run(f"layer1/layer2 overlap gap {gap}", overlap("layer1", "layer2", gap)))
    res.append(run(f"flash/number gap {gap}", overlap("flash", "3", gap)))
rng = random.Random(int(sys.argv[2]) if len(sys.argv) > 2 else 1)
pool = ["1", "2", "3", "4", "palette1", "palette2", "palette3", "layer1", "layer2", "flash", "blackout"]
for i in range(int(sys.argv[3]) if len(sys.argv) > 3 else 15):
    res.append(run(f"random #{i}", rand_seq(pool, 24, rng)))
print(f"{sum(res)}/{len(res)} passed")
