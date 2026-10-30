# Layer stress test on the brain. Each case is a list of layers (fx, blend mode, on, frozen) stacked
# on top of segment 0; segment 0 keeps fx 0 (Solid) unless the case says otherwise.
import json, sys, time, statistics, urllib.request
B = "http://4.3.2.2"
def get(p): return json.load(urllib.request.urlopen(B + p, timeout=5))
def post(obj): urllib.request.urlopen(urllib.request.Request(B + "/json/state", json.dumps(obj).encode(), {"Content-Type": "application/json"}), timeout=8).read()
def fps(n=10, gap=0.4):
    v = []
    for _ in range(n):
        try: v.append(get("/json/info")["leds"]["fps"])
        except Exception: pass
        time.sleep(gap)
    return statistics.median(v) if v else 0
MAXL = 8
def setup(base_fx, layers):
    segs = [{"id": 0, "fx": base_fx, "fxdef": True, "on": True, "frz": False}]
    for i in range(1, MAXL + 1):
        if i <= len(layers):
            fx, bm, on, frz = layers[i - 1]
            segs.append({"id": i, "start": 0, "stop": 95, "startY": 0, "stopY": 41, "fx": fx, "fxdef": True,
                         "bm": bm, "on": on, "frz": frz, "bri": 255})
        else:
            segs.append({"id": i, "stop": 0})
    post({"tt": 0, "seg": segs})
cases = json.load(open(sys.argv[1]))
out = open(sys.argv[2], "a")
for c in cases:
    setup(c.get("base", 0), c["layers"])
    time.sleep(4)
    f = fps()
    seen = len(get("/json/state")["seg"])
    line = {"name": c["name"], "fps": f, "ms": round(1000 / f, 2) if f else None, "segments": seen}
    print(json.dumps(line), flush=True); out.write(json.dumps(line) + "\n"); out.flush()
