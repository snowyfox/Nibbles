// Nibbles lighting simulator page.
// Sound (mic or file) -> WLED AudioReactive's processing (WebAssembly) ->
// audio sync packets -> server.py -> the simulator WLED; its LED output comes
// back over DDP -> server.py -> this page, drawn at each LED's position.
'use strict';

const $ = (id) => document.getElementById(id);
const AR_RATE = 22050, AR_BLOCK = 512;
const SHARK_AR = { squelch: 10, gain: 30, agc: 2, level: 128 };  // the shark's AudioReactive settings

let sim, ws, audioCtx, tapNode, source;
let blockPtr, packetPtr, outPtr;
let pending = new Float32Array(0), readPos = 0;
let layout = null, leds = null, view = '3d';
// WLED sends LED drive levels (light output ~ proportional to the value); a
// screen shows values on a gamma curve, so faint LEDs look darker than on the
// shark. "Dim boost" lifts them: 0 shows the raw values, 50% is the true
// conversion (gamma 1/2.2), more lifts faint light further. Too much hides
// effects with a faint background, where the moving part no longer stands out.
const toScreen = new Uint8Array(256);
function setDimBoost(boost) {  // 0..1
  const g = 1 / (1 + boost * 2.4);
  for (let v = 0; v < 256; v++) toScreen[v] = v ? Math.max(1, Math.round(255 * Math.pow(v / 255, g))) : 0;
}
setDimBoost(0.25);
let ledScale = 1.5;  // LED size on screen (the LED size slider)
window.nibblesLedScale = ledScale;  // for view3d.js
let canvas3d = false;                      // no WebGL: draw the 3D view on the 2D canvas
// Default view: level with the shark (its XY plane edge-on), from the
// starboard front quarter: X points left and Y (the nose) right, both 45°
// towards you.
const HOME_VIEW = { yaw: 3 * Math.PI / 4, pitch: 0, zoom: 1 };
const orbit = { ...HOME_VIEW };
let frames = 0;

function status(text) { $('status').textContent = text; }

// ------------------------------------------------------------------ WebAssembly + WebSocket

async function start() {
  sim = await SimModule();
  sim._sim_ar_init(SHARK_AR.squelch, SHARK_AR.gain, SHARK_AR.agc, SHARK_AR.level);
  blockPtr = sim._sim_block_buffer();
  packetPtr = sim._sim_packet();
  outPtr = sim._sim_ar_output();
  eyesStart();
  connect();
  await loadLayout();
  // view3d.js is a module and may load after this script
  if (window.nibbles3d) showView();
  else window.addEventListener('load', showView);
  loadWled();
  requestAnimationFrame(draw);
  setInterval(() => { $('fps').textContent = frames; frames = 0; }, 1000);
  status('ready: pick a sound source');
}

function connect() {
  ws = new WebSocket(`ws://${location.host}/ws`);
  ws.binaryType = 'arraybuffer';
  ws.onmessage = (e) => {
    if (typeof e.data === 'string') {  // the lights' source, or the live shark's state
      const m = JSON.parse(e.data);
      if ('source' in m) showSource(m);
      else eyesFromShark(m);
      return;
    }
    leds = new Uint8Array(e.data);
    for (let i = 0; i < leds.length; i++) leds[i] = toScreen[leds[i]];
    frames++;
    if (view === '3d' && window.nibbles3d) window.nibbles3d.update(leds);
  };
  ws.onclose = () => { status('lost the simulator server; retrying…'); setTimeout(connect, 1000); };
}

// ------------------------------------------------------------------ sound

async function ensureAudio() {
  if (audioCtx) return;
  audioCtx = new AudioContext();
  await audioCtx.audioWorklet.addModule('worklet.js');
  tapNode = new AudioWorkletNode(audioCtx, 'tap');
  tapNode.port.onmessage = (e) => { feed(e.data); eyesFeed(e.data); };
}

function useSource(node, audible) {
  if (source) source.disconnect();
  source = node;
  source.connect(tapNode);
  if (audible) source.connect(audioCtx.destination);
}

$('mic').onclick = async () => {
  await ensureAudio();
  const stream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
  useSource(audioCtx.createMediaStreamSource(stream), false);
  $('mic').classList.add('on');
  $('player').hidden = true;
  $('player').pause();
  status('listening to the microphone');
};

let playerNode;
$('pick').onclick = () => $('file').click();
$('file').onchange = async () => {
  const f = $('file').files[0];
  if (!f) return;
  await ensureAudio();
  const player = $('player');
  player.src = URL.createObjectURL(f);
  player.hidden = false;
  if (!playerNode) playerNode = audioCtx.createMediaElementSource(player);
  useSource(playerNode, true);
  $('mic').classList.remove('on');
  await audioCtx.resume();
  player.play();
  status(`playing ${f.name}`);
};

$('gain').oninput = () => { $('gainv').textContent = `×${(+$('gain').value).toFixed(2)}`; };
$('gain').oninput();

// Resample to 22050 Hz, cut into 512-sample blocks, run AudioReactive's
// processing on each and send the packet to WLED.
function feed(chunk) {
  const merged = new Float32Array(pending.length + chunk.length);
  merged.set(pending);
  merged.set(chunk, pending.length);
  pending = merged;
  const step = audioCtx.sampleRate / AR_RATE, scale = 32767 * +$('gain').value;
  const block = sim.HEAPF32.subarray(blockPtr >> 2, (blockPtr >> 2) + AR_BLOCK);
  if (feed.fill === undefined) feed.fill = 0;
  while (readPos + 1 < pending.length) {
    const i = Math.floor(readPos), f = readPos - i;
    block[feed.fill++] = (pending[i] * (1 - f) + pending[i + 1] * f) * scale;
    readPos += step;
    if (feed.fill === AR_BLOCK) {
      feed.fill = 0;
      sim._sim_ar_block();
      eyesAR();
      if (ws.readyState === 1) ws.send(sim.HEAPU8.slice(packetPtr, packetPtr + 44));
      showAudio();
    }
  }
  const keep = Math.floor(readPos);
  pending = pending.slice(keep);
  readPos -= keep;
}

function showAudio() {
  const f = sim.HEAPF32, b = outPtr >> 2;  // ar_output_t: 5 floats, bool, 16 bytes
  $('vol').textContent = f[b].toFixed(0);
  $('agc').textContent = f[b + 4].toFixed(2);
  $('freq').textContent = `${f[b + 3].toFixed(0)} Hz`;
  const peak = sim.HEAPU8[outPtr + 20];
  $('peak').style.visibility = peak ? 'visible' : 'hidden';
  const bands = sim.HEAPU8.subarray(outPtr + 21, outPtr + 37);
  const c = $('bands'), g = c.getContext('2d'), w = c.width / 16;
  g.clearRect(0, 0, c.width, c.height);
  for (let i = 0; i < 16; i++) {
    const h = bands[i] / 255 * c.height;
    g.fillStyle = `hsl(${300 - i * 14}, 90%, 60%)`;
    g.fillRect(i * w + 1, c.height - h, w - 2, h);
  }
}

// ------------------------------------------------------------------ WLED control

const wled = () => `http://${$('wledip').value.trim()}`;

// Where the lights come from: the bench simulator WLED (driven by the sound
// above), or the shark's own controller, mirrored live (server.py).
let config = { bench: '10.7.200.226', shark: '10.7.200.253' };
fetch('config.json').then((r) => r.json()).then((c) => { config = { ...config, ...c }; }).catch(() => {});
let lightsFrom = null;  // 'sim' or 'shark'
function showSource(m) {
  const changed = m.source !== lightsFrom;
  lightsFrom = m.source;
  $('source').value = m.source;
  const live = m.source === 'shark';
  $('wledtitle').textContent = live ? 'WLED (THE SHARK, LIVE: CONTROLS CHANGE IT)' : 'WLED (BENCH SIMULATOR)';
  $('wledip').value = live ? m.ip : config.bench;
  if (changed) {
    loadWled();
    eyesFollowShark(live);
    status(live ? `showing the shark live (${m.ip})` : 'showing the bench simulator');
  }
}
$('source').onchange = () => {
  const live = $('source').value === 'shark';
  ws.send(JSON.stringify({ source: $('source').value, ip: live ? ($('wledip').value.trim() !== config.bench ? $('wledip').value.trim() : config.shark) : null }));
};

async function wledPost(state) {
  // text/plain keeps it a "simple" request (no CORS preflight); WLED reads the JSON anyway.
  await fetch(`${wled()}/json/state`, { method: 'POST', body: JSON.stringify(state) });
}

async function loadWled() {
  try {
    const [presets, state] = await Promise.all([
      fetch(`${wled()}/presets.json`).then((r) => r.json()),
      fetch(`${wled()}/json/state`).then((r) => r.json()),
    ]);
    const sel = $('preset');
    sel.innerHTML = '<option value="">–</option>';
    Object.entries(presets).filter(([id, p]) => +id > 0 && p.n)
      .sort((a, b) => a[1].n.localeCompare(b[1].n))
      .forEach(([id, p]) => sel.add(new Option(`${p.n} (${id})`, id)));
    sel.value = state.ps > 0 ? String(state.ps) : '';
    showWledState(state);
  } catch (e) {
    $('preset').innerHTML = '<option>WLED not reachable</option>';
  }
}

$('preset').onchange = () => { if ($('preset').value) wledPost({ ps: +$('preset').value }); };
$('bri').oninput = () => {
  $('briv').textContent = $('bri').value;
  wledOn = true;  // WLED turns on when given a brightness
  eyesWledBrightness(wledOn, +$('bri').value);
  wledPost({ bri: +$('bri').value });
};
// WLED's brightness and on/off, which the eyes follow as on the shark.
let wledOn = true;
function showWledState(state) {
  if (document.activeElement !== $('bri')) { $('bri').value = state.bri; $('briv').textContent = state.bri; }
  wledOn = state.on;
  eyesWledBrightness(state.on, state.bri);
}
// The bench WLED can be changed elsewhere (its own page); check now and then.
// (The live shark's state arrives with every mirrored frame.)
setInterval(() => {
  if (lightsFrom === 'shark') return;
  fetch(`${wled()}/json/state`).then((r) => r.json()).then(showWledState).catch(() => {});
}, 2000);
$('wledip').onchange = loadWled;

// ------------------------------------------------------------------ LED layout and drawing

// Until the real layout exists: one row per WLED output channel.
function placeholderLayout() {
  const channels = [418, 417, 114, 115, 130];
  const points = [];
  channels.forEach((n, row) => {
    for (let i = 0; i < n; i++) points.push([i * 10, -row * 200, 0]);
  });
  return { name: 'placeholder: one row per output channel', leds: points };
}

async function loadLayout() {
  try {
    const r = await fetch('layout.json');
    if (!r.ok) throw new Error();
    layout = await r.json();
    if (layout.up === 'z') {  // make y the up axis (and z point to the tail)
      const yUp = ([x, y, z]) => [x, z, -y];
      layout.leds = layout.leds.map(yUp);
      layout.outlines = (layout.outlines || []).map((line) => line.map(yUp));
      if (layout.pole) layout.pole.line = [yUp(layout.pole.from), yUp(layout.pole.to)];
      // Channels the layout marks hidden keep their place in WLED's data but aren't drawn.
      layout.hidden = new Uint8Array(layout.leds.length);
      for (const ch of layout.channels || []) if (ch.hidden) layout.hidden.fill(1, ch.start, ch.start + ch.count);
      for (const e of layout.eyes || []) {  // directions turn the same way as positions
        e.centre = yUp(e.centre); e.normal = yUp(e.normal); e.right = yUp(e.right);
        e.up = cross(e.normal, e.right);
      }
    }
  } catch (e) {
    layout = placeholderLayout();
  }
  $('layoutname').textContent = `· ${layout.name || 'layout'}`;
  $('count').textContent = layout.leds.length;
}

function showView() {
  const is3d = view === '3d';
  $('viewnote').hidden = !(is3d && canvas3d);
  if (!is3d || canvas3d || !window.nibbles3d || !layout) {
    if (window.nibbles3d) window.nibbles3d.hide();
    $('ledview').hidden = false;
    return;
  }
  try {
    window.nibbles3d.show(layout);
    $('ledview').hidden = true;
  } catch (e) {  // e.g. no WebGL: draw the 3D view without it (LEDs and tubes; no body)
    console.error('WebGL 3D view failed:', e);
    canvas3d = true;
    window.nibbles3d.hide();
    $('ledview').hidden = false;
    $('viewnote').hidden = false;
    $('viewnote').textContent = 'simple 3D (no WebGL here): drag to turn, scroll to zoom; tools/sim/open_chrome.sh shows the body too';
  }
}

// Orbit the simple 3D view: drag to turn, scroll to zoom.
(() => {
  const c = $('ledview');
  let drag = null;
  c.addEventListener('pointerdown', (e) => { drag = { x: e.clientX, y: e.clientY }; c.setPointerCapture(e.pointerId); });
  c.addEventListener('pointermove', (e) => {
    if (!drag) return;
    orbit.yaw += (e.clientX - drag.x) * 0.01;
    orbit.pitch = Math.max(-1.5, Math.min(1.5, orbit.pitch + (e.clientY - drag.y) * 0.01));
    drag = { x: e.clientX, y: e.clientY };
  });
  c.addEventListener('pointerup', () => { drag = null; });
  c.addEventListener('wheel', (e) => { e.preventDefault(); orbit.zoom *= Math.exp(-e.deltaY * 0.001); }, { passive: false });
})();

document.querySelectorAll('.views button').forEach((b) => {
  b.onclick = () => {
    headFocus = b.dataset.view === 'head';  // the 3D view, close up on the eyes
    view = headFocus ? '3d' : b.dataset.view;
    if (view === '3d') focus3d();
    document.querySelectorAll('.views button').forEach((x) => x.classList.toggle('on', x === b));
    showView();
  };
});

let centre = null, radius3d = 1, headFocus = false;
const EYE_BACK_DIM = 0.4;  // an eye screen seen from behind: this bright
// Where the 3D views look: the whole shark, or the eyes (Head).
function focus3d() {
  const eyes = (layout && layout.eyes) || [];
  if (headFocus && eyes.length) {
    const mid = [0, 1, 2].map((k) => eyes.reduce((a, e) => a + e.centre[k], 0) / eyes.length);
    if (window.nibbles3d && window.nibbles3d.focus) window.nibbles3d.focus(mid, 190);
    Object.assign(orbit, { yaw: 1.1, pitch: 0.15, zoom: 1 });  // the starboard eye, nose to the right
    focus3d.pivot = mid; focus3d.radius = 45;
  } else {
    if (window.nibbles3d && window.nibbles3d.focus) window.nibbles3d.focus(null);
    Object.assign(orbit, HOME_VIEW);
    focus3d.pivot = null;
  }
}
const cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
// The simple 3D view's orbit: a direction turned to the camera's frame
// (x right, y up, z towards the viewer).
function turn([x, y, z]) {
  const cyw = Math.cos(orbit.yaw), syw = Math.sin(orbit.yaw), cp = Math.cos(orbit.pitch), sp = Math.sin(orbit.pitch);
  [x, z] = [x * cyw - z * syw, x * syw + z * cyw];
  [y, z] = [y * cp - z * sp, y * sp + z * cp];
  return [x, y, z];
}
// Does a surface with this normal face the viewer?
function facing(n) {
  if (view === '3d') return turn(n)[2] > 0;
  if (view === 'side') return n[0] > 0;   // side view looks at the starboard side
  if (view === 'top') return n[1] > 0;
  return n[2] < 0;                        // front view looks at the nose
}
function project(p) {
  if (view === '3d') {  // simple perspective orbit (the WebGL-less 3D view)
    const c = focus3d.pivot || centre;
    const [x, y, z] = turn([p[0] - c[0], p[1] - c[1], p[2] - c[2]]);
    const f = 1400 / (1400 - z);  // camera 1.4 m from the middle
    return [x * f, y * f];
  }
  // front: facing the nose (starboard on the left); top: from above, nose up;
  // side: the starboard side, nose to the right
  if (view === 'top') return [p[0], -p[2]];
  if (view === 'side') return [-p[2], p[1]];
  return [-p[0], p[1]];
}

// The body: a thin sheet in the plane x = 0, filling the main outline tube.
// LEDs seen through it (the far fin, mostly) are drawn at BODY_DIM brightness.
const BODY_DIM = 0.2;
function bodyPolygon() {
  if (bodyPolygon.cache && bodyPolygon.cache.layout === layout) return bodyPolygon.cache.poly;
  const poly = ((layout.outlines || [])[0] || []).map((p) => [p[1], p[2]]);
  bodyPolygon.cache = { layout, poly };
  return poly;
}
function insidePolygon(y, z, poly) {
  let inside = false;
  for (let i = 0, j = poly.length - 1; i < poly.length; j = i++) {
    const [yi, zi] = poly[i], [yj, zj] = poly[j];
    if ((zi > z) !== (zj > z) && y < yi + (z - zi) * (yj - yi) / (zj - zi)) inside = !inside;
  }
  return inside;
}
// Is LED p (page coordinates) hidden behind the body, seen along w (a
// direction from p towards the viewer)? Strips in the body's own plane never are.
function behindBody(p, w) {
  if (!layout || Math.abs(p[0]) < 4 || Math.abs(w[0]) < 1e-6) return false;
  const t = -p[0] / w[0];
  if (t <= 0) return false;  // on the viewer's side of the body
  return insidePolygon(p[1] + t * w[1], p[2] + t * w[2], bodyPolygon());
}
window.nibblesBehindBody = behindBody;  // for view3d.js

// The tubes cut into short pieces (3D, the page's y-up mm), for
// drawing in depth order in the canvas views.
function tubePieces() {
  if (tubePieces.cache && tubePieces.cache.layout === layout) return tubePieces.cache.pieces;
  const pieces = [];
  const add = (line, width, step) => {
    for (let i = 0; i + 1 < line.length; i++) {
      const a = line[i], b = line[i + 1];
      const n = Math.max(1, Math.ceil(Math.hypot(b[0] - a[0], b[1] - a[1], b[2] - a[2]) / step));
      for (let k = 0; k < n; k++) {
        const p = a.map((v, j) => v + (b[j] - v) * k / n), q = a.map((v, j) => v + (b[j] - v) * (k + 1) / n);
        pieces.push({ a: p, b: q, mid: p.map((v, j) => (v + q[j]) / 2), width });
      }
    }
  };
  for (const line of layout.outlines || []) add(line, 13, 6);  // tube diameter
  tubePieces.cache = { layout, pieces };
  return pieces;
}

function draw() {
  if (view === '3d' && !canvas3d && window.nibbles3d) { requestAnimationFrame(draw); return; }
  if (layout && !centre) {  // middle of the LEDs, the orbit's pivot
    const n = layout.leds.length;
    centre = [0, 1, 2].map((k) => layout.leds.reduce((a, p) => a + p[k], 0) / n);
    radius3d = Math.max(...layout.leds.map((p) => Math.hypot(p[0] - centre[0], p[1] - centre[1], p[2] - centre[2])));
  }
  const c = $('ledview');
  const dpr = window.devicePixelRatio || 1;
  if (c.width !== c.clientWidth * dpr || c.height !== c.clientHeight * dpr) {
    c.width = c.clientWidth * dpr;
    c.height = c.clientHeight * dpr;
  }
  const g = c.getContext('2d');
  g.globalCompositeOperation = 'source-over';
  g.fillStyle = '#000';
  g.fillRect(0, 0, c.width, c.height);
  if (layout) {
    const pts = layout.leds.map(project);
    const extras = (layout.outlines || []).map((line) => line.map(project));
    let x0 = Infinity, x1 = -Infinity, y0 = Infinity, y1 = -Infinity;
    for (const [x, y] of pts.concat(...extras)) { x0 = Math.min(x0, x); x1 = Math.max(x1, x); y0 = Math.min(y0, y); y1 = Math.max(y1, y); }
    const pad = 30 * dpr;
    let s = Math.min((c.width - 2 * pad) / (x1 - x0 || 1), (c.height - 2 * pad) / (y1 - y0 || 1));
    let ox = (c.width - (x1 - x0) * s) / 2, oy = (c.height - (y1 - y0) * s) / 2;
    if (view === '3d') {  // fixed scale round the pivot, so orbiting doesn't jump
      s = Math.min(c.width, c.height) / (2 * (focus3d.pivot ? focus3d.radius : radius3d) * 1.1) * orbit.zoom;
      x0 = y0 = 0;
      ox = c.width / 2; oy = c.height / 2;
    }
    const r = Math.max(1.2 * dpr, Math.min(6 * dpr, s * 2)) * ledScale;
    const X = (p) => ox + (p[0] - x0) * s, Y = (p) => c.height - (oy + (p[1] - y0) * s);
    // What gets drawn, far to near (painter's order), so nearer things cover
    // farther ones: the grey tubes are opaque, the LEDs glow
    // (added light), the eye screens are opaque discs. A tube piece sorts as
    // if TUBE_R further back, so it never covers the LEDs inside it.
    const TUBE_R = 6.5;
    const pieces = tubePieces();
    const depth = (p) => -viewDir(p)[2];  // bigger = further from the viewer
    // Screens: each LED or tube piece goes just after the last screen it is in
    // front of (on the viewer's side of that screen's plane).
    const eyes = (layout.eyes || []).slice().sort((a, b) => facing(a.normal) - facing(b.normal));
    const layerOf = (p) => {
      let k = 0;
      eyes.forEach((e, i) => {
        const d = (p[0] - e.centre[0]) * e.normal[0] + (p[1] - e.centre[1]) * e.normal[1] + (p[2] - e.centre[2]) * e.normal[2];
        if ((d > 0) === facing(e.normal)) k = i + 1;
      });
      return k;
    };
    const items = [];  // [layer, depth, kind (0 tube, 1 LED), index]
    pieces.forEach((q, i) => items.push([layerOf(q.mid), depth(q.mid) + TUBE_R, 0, i]));
    for (let i = 0; i < pts.length; i++) {
      if (layout.hidden[i]) continue;
      const R = leds ? leds[i * 3] : 20, G = leds ? leds[i * 3 + 1] : 20, B = leds ? leds[i * 3 + 2] : 20;
      if (R | G | B) items.push([layerOf(layout.leds[i]), depth(layout.leds[i]), 1, i]);
    }
    items.sort((a, b) => a[0] - b[0] || b[1] - a[1]);
    const toViewer = [viewDir([1, 0, 0])[2], viewDir([0, 1, 0])[2], viewDir([0, 0, 1])[2]];
    g.strokeStyle = '#2a2f3a';
    g.lineCap = 'round';
    let next = 0;
    const drawLayer = (k) => {
      for (; next < items.length && items[next][0] === k; next++) {
        const [, , kind, i] = items[next];
        if (kind === 0) {
          const q = pieces[i], a = project(q.a), b = project(q.b);
          g.globalCompositeOperation = 'source-over';
          g.lineWidth = q.width * s;
          g.beginPath(); g.moveTo(X(a), Y(a)); g.lineTo(X(b), Y(b)); g.stroke();
        } else {
          const k = behindBody(layout.leds[i], toViewer) ? BODY_DIM : 1;
          const R = Math.round((leds ? leds[i * 3] : 20) * k), G = Math.round((leds ? leds[i * 3 + 1] : 20) * k),
            B = Math.round((leds ? leds[i * 3 + 2] : 20) * k);
          const x = X(pts[i]), y = Y(pts[i]);
          g.globalCompositeOperation = 'lighter';
          g.fillStyle = `rgba(${R},${G},${B},0.25)`;  // soft glow, like the silicone diffuser
          g.beginPath(); g.arc(x, y, r * 2.2, 0, 6.2832); g.fill();
          g.fillStyle = `rgb(${R},${G},${B})`;
          g.beginPath(); g.arc(x, y, r, 0, 6.2832); g.fill();
        }
      }
    };
    // The eyes' screens: each eye canvas mapped onto its disc (an affine map,
    // near enough at this size). Seen from behind they show mirrored (the map
    // does that) and dimmer.
    drawLayer(0);
    eyes.forEach((e, k) => {
      const img = $(e.side === 'port' ? 'eye_port' : 'eye_sb');
      if (img.width) {
        const at = (m, v) => { const q = project(e.centre.map((c, i) => c + m * v[i] * e.radius_mm)); return [X(q), Y(q)]; };
        const c0 = at(0, e.right), cr = at(1, e.right), cu = at(1, e.up);
        const A = [cr[0] - c0[0], cr[1] - c0[1]], B = [cu[0] - c0[0], cu[1] - c0[1]], h = img.width / 2;
        g.globalCompositeOperation = 'source-over';
        g.save();
        g.setTransform(A[0] / h, A[1] / h, -B[0] / h, -B[1] / h, c0[0] - A[0] + B[0], c0[1] - A[1] + B[1]);
        g.beginPath(); g.arc(h, h, h, 0, 6.2832); g.clip();
        g.drawImage(img, 0, 0);
        if (!facing(e.normal)) { g.fillStyle = `rgba(0,0,0,${1 - EYE_BACK_DIM})`; g.fill(); }
        g.restore();
      }
      drawLayer(k + 1);
    });
  }
  requestAnimationFrame(draw);
}

start();

// ------------------------------------------------------------------ axis indicator

// The model's axes (design coordinates, as in Fusion: X starboard, Y nose,
// Z up), drawn in the corner the way the current view sees them.
const AXES = [
  { name: 'X', v: [1, 0, 0], color: '#ff5555' },   // starboard
  { name: 'Y', v: [0, 0, -1], color: '#55dd55' },  // nose (design +Y, in the page's y-up coordinates)
  { name: 'Z', v: [0, 1, 0], color: '#5599ff' },   // up
];
// A direction as the current view shows it: [right, up, towards the viewer].
function viewDir(v) {
  if (view === '3d' && !canvas3d && window.nibbles3d && window.nibbles3d.viewDir) return window.nibbles3d.viewDir(v);
  if (view === '3d') return turn(v);
  if (view === 'top') return [v[0], -v[2], v[1]];
  if (view === 'side') return [-v[2], v[1], v[0]];
  return [-v[0], v[1], -v[2]];
}
function drawGizmo() {
  requestAnimationFrame(drawGizmo);
  const c = $('gizmo'), dpr = window.devicePixelRatio || 1, size = 90 * dpr;
  if (c.width !== size) c.width = c.height = size;
  const g = c.getContext('2d');
  g.clearRect(0, 0, size, size);
  const m = size / 2, len = size * 0.34;
  const axes = AXES.map((a) => ({ ...a, d: viewDir(a.v) })).filter((a) => a.d);
  axes.sort((a, b) => a.d[2] - b.d[2]);  // away from the viewer first
  g.fillStyle = 'rgba(20,22,28,0.55)';
  g.beginPath(); g.arc(m, m, size * 0.48, 0, 6.2832); g.fill();
  g.lineCap = 'round';
  for (const a of axes) {
    const [x, y, z] = a.d;
    const ex = m + x * len, ey = m - y * len;
    const alpha = z < -0.2 ? 0.55 : 1;  // pointing away: fainter
    g.globalAlpha = alpha;
    g.strokeStyle = g.fillStyle = a.color;
    g.lineWidth = 3 * dpr;
    g.beginPath(); g.moveTo(m, m); g.lineTo(ex, ey); g.stroke();
    // arrow head
    const L = Math.hypot(x, y);
    if (L > 0.05) {
      const ux = x / L, uy = -y / L, hs = 7 * dpr;
      g.beginPath();
      g.moveTo(ex + ux * hs * 0.6, ey + uy * hs * 0.6);
      g.lineTo(ex - ux * hs * 0.6 - uy * hs * 0.5, ey - uy * hs * 0.6 + ux * hs * 0.5);
      g.lineTo(ex - ux * hs * 0.6 + uy * hs * 0.5, ey - uy * hs * 0.6 - ux * hs * 0.5);
      g.closePath(); g.fill();
    }
    // label just past the tip (or a dot when the axis points straight at or away from us)
    const lx = m + x * (len + 11 * dpr), ly = m - y * (len + 11 * dpr);
    g.font = `bold ${11 * dpr}px -apple-system, sans-serif`;
    g.textAlign = 'center'; g.textBaseline = 'middle';
    if (L > 0.05) g.fillText(a.name, lx, ly);
    else { g.beginPath(); g.arc(m, m, 4 * dpr, 0, 6.2832); g.fill(); g.fillText(a.name, m + 9 * dpr, m - 9 * dpr); }
  }
  g.globalAlpha = 1;
  g.fillStyle = '#ddd';
  g.beginPath(); g.arc(m, m, 2.5 * dpr, 0, 6.2832); g.fill();
}
requestAnimationFrame(drawGizmo);

// LED size slider (remembered in this browser)
(() => {
  const b = $('ledsize');
  try { const v = localStorage.getItem('nibbles.ledSize'); if (v !== null) b.value = v; } catch (e) {}
  const apply = () => {
    ledScale = window.nibblesLedScale = +b.value;
    $('ledsizev').textContent = `×${(+b.value).toFixed(1)}`;
    try { localStorage.setItem('nibbles.ledSize', b.value); } catch (e) {}
  };
  b.oninput = apply;
  apply();
})();

// Dim boost slider (remembered in this browser)
(() => {
  const b = $('boost');
  try { const v = localStorage.getItem('nibbles.dimBoost2'); if (v !== null) b.value = v; } catch (e) {}
  const apply = () => {
    setDimBoost(+b.value);
    $('boostv').textContent = Math.round(b.value * 100) + '%';
    try { localStorage.setItem('nibbles.dimBoost2', b.value); } catch (e) {}
  };
  b.oninput = apply;
  apply();
})();
