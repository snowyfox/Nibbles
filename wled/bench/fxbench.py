# Time every WLED effect on the brain and measure how long it takes to "spin up" from black.
# Frames come from the simulator server's WebSocket (it mirrors the brain's LEDs over DDP).
import json, sys, time, statistics, urllib.request, socket, base64, os, threading
B = "http://4.3.2.2"
COL = [[255, 0, 255], [0, 0, 0], [0, 0, 0]]   # the brain's colours before the test
def get(p): return json.load(urllib.request.urlopen(B + p, timeout=5))
def post(obj): urllib.request.urlopen(urllib.request.Request(B + "/json/state", json.dumps(obj).encode(), {"Content-Type": "application/json"}), timeout=5).read()

frames = []   # (time, mean level 0-255, lit fraction)
def ws_reader():
    s = socket.create_connection(("127.0.0.1", 8080))
    key = base64.b64encode(os.urandom(16)).decode()
    s.sendall(f"GET /ws HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n".encode())
    buf = b""
    while b"\r\n\r\n" not in buf: buf += s.recv(4096)
    buf = buf.split(b"\r\n\r\n", 1)[1]
    def need(n):
        nonlocal buf
        while len(buf) < n:
            d = s.recv(65536)
            if not d: raise EOFError
            buf += d
    while True:
        need(2); op = buf[0] & 0x0F; n = buf[1] & 0x7F; h = 2
        if n == 126: need(4); n = int.from_bytes(buf[2:4], "big"); h = 4
        elif n == 127: need(10); n = int.from_bytes(buf[2:10], "big"); h = 10
        need(h + n); data = buf[h:h + n]; buf = buf[h + n:]
        if op == 2 and len(data) >= 300:
            lv = [max(data[i], data[i + 1], data[i + 2]) for i in range(0, len(data) - 2, 3)]
            frames.append((time.time(), sum(lv) / len(lv), sum(1 for v in lv if v > 16) / len(lv)))
threading.Thread(target=ws_reader, daemon=True).start()

def fps(n=10, gap=0.3):
    v = []
    for _ in range(n):
        try: v.append(get("/json/info")["leds"]["fps"])
        except Exception: pass
        time.sleep(gap)
    return statistics.median(v) if v else 0, (min(v), max(v)) if v else (0, 0)

def spinup(t0):
    fr = [f for f in frames if f[0] >= t0]
    if len(fr) < 20: return None, None, None, len(fr)
    steady = [f for f in fr if f[0] - t0 >= 3.0]
    lvl = statistics.median(f[1] for f in steady) if steady else 0
    lit = statistics.median(f[2] for f in steady) if steady else 0
    if lvl < 2: return 0, round(lvl, 1), round(lit, 3), len(fr)   # stays dark (silent audio effects, etc.)
    for i in range(len(fr) - 2):   # first moment three frames in a row reach 80% of the steady level
        if all(fr[j][1] >= 0.8 * lvl for j in range(i, i + 3)):
            return round(fr[i][0] - t0, 2), round(lvl, 1), round(lit, 3), len(fr)
    return None, round(lvl, 1), round(lit, 3), len(fr)

time.sleep(2)
names = get("/json/eff")
start = int(sys.argv[2]) if len(sys.argv) > 2 else 0
out = open(sys.argv[1], "a")
if start == 0: out.write("fx,name,fps,min,max,spinup_s,steady_level,steady_lit,frames\n")
done = 0
for fx, name in enumerate(names):
    if fx < start or name == "RSVD" or name.startswith("-"): continue
    if done % 15 == 0 and fx != 0:
        post({"tt": 0, "seg": {"id": 0, "fx": 0, "col": COL}}); time.sleep(2.5)
        m, (lo, hi) = fps(); out.write(f'ref,"Solid (reference)",{m},{lo},{hi},,,,\n'); print("ref", m, flush=True)
    done += 1
    try:
        post({"tt": 0, "seg": {"id": 0, "fx": 0, "col": [[0, 0, 0], [0, 0, 0], [0, 0, 0]]}})   # black first
        time.sleep(0.8)
        post({"tt": 0, "seg": {"id": 0, "fx": fx, "fxdef": True, "col": COL}})
    except Exception as e: print("fail", fx, e, flush=True); continue
    t0 = time.time()
    time.sleep(2.2)
    m, (lo, hi) = fps()          # 2.2 s .. ~5.5 s after the start
    sp, lvl, lit, n = spinup(t0)
    frames[:] = [f for f in frames if f[0] > time.time() - 1]
    row = [fx, f'"{name}"', m, lo, hi, "" if sp is None else sp, lvl, lit, n]
    out.write(",".join(map(str, row)) + "\n"); out.flush()
    print(*row, flush=True)
