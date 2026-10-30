// The base station: its keypad and screen. The keys run through the base
// firmware's own Live / Standby logic (controls.c, in server.py); this page
// draws its screen like firmware/base/main/display.c's main page and carries
// out the actions: WLED over its JSON API (as the WLED card does), the
// simulated eyes directly. Uses app.js's and eyes.js's globals.
'use strict';

const BASE_COLS = [
  { title: 'Numbers', keys: [['1', '1'], ['2', '2'], ['3', '3'], ['4', '4'], ['5', '5'], ['6', '6'], ['7', '7'], ['8', '8']] },
  { title: 'Functions', keys: [['all', 'ALL'], ['body', 'BODY'], ['eyes', 'EYES'], ['fins', 'FINS'], ['page', 'PAGE'],
                               ['latch', 'LATCH'], ['blackout', 'BLACK OUT'], ['finsensor', 'FIN SENSOR']] },
  { title: 'Pole (upper)', keys: [['take', 'TAKE'], ['flash', 'FLASH'], ['unlock', 'UNLOCK']] },
];
// Computer keyboard: hold like the real keys. Letters, digits, Space and Shift
// by the character they type (so Dvorak and other layouts get the letter on
// the key cap)...
const BASE_KEYS_TYPED = { '1': '1', '2': '2', '3': '3', '4': '4', '5': '5', '6': '6', '7': '7', '8': '8',
  a: 'all', b: 'body', e: 'eyes', f: 'fins', p: 'page', shift: 'latch', l: 'latch',
  z: 'blackout', s: 'finsensor', ' ': 'take', x: 'flash', u: 'unlock' };
// ...and the number pad by physical key, which is the same in every layout (and
// Num Lock doesn't matter): 1-8 the numbers, 0 TAKE, Enter LATCH (both big keys,
// easy to hold and tap together), / * - = the targets, + PAGE, . FLASH, 9 BLACKOUT.
const BASE_NUMPAD = {
  Numpad1: '1', Numpad2: '2', Numpad3: '3', Numpad4: '4', Numpad5: '5', Numpad6: '6', Numpad7: '7', Numpad8: '8',
  Numpad0: 'take', NumpadEnter: 'latch', NumpadDivide: 'body', NumpadMultiply: 'eyes', NumpadSubtract: 'all',
  NumpadEqual: 'fins', NumLock: 'fins', NumpadAdd: 'page', NumpadDecimal: 'flash', Numpad9: 'blackout' };
const ACT = { PRESET: 0, FINS: 1, BRIGHTNESS: 2, BUMP: 3, BUMP_END: 4, SLOT_LOOK: 5, SLOT_PALETTE: 6 };
const SLOT = { BODY: 0, FINS: 1, BODY_OVER: 2, FINS_OVER: 3 };   // WLED's layer slots (nl_slot_t)
const LAYER_BASE = 100;   // NL_LAYER_PRESET_BASE
const TGT = { ALL: 0, BODY: 1, EYES: 2, FINS: 3 };
const BUMP_NAMES = { 1: 'FLASH', 2: 'BLACKOUT', 3: 'PRESET', 4: 'PALETTE', 5: 'LAYER' };

// Fin presets: WLED preset FIN_BASE + n (FIN_BASE itself: the fins follow the body); see nibbles_link.h.
const FIN_BASE = 80, FIN_COUNT = 16;
const isFinPreset = (id) => id >= FIN_BASE && id <= FIN_BASE + FIN_COUNT;
let baseState = null, baseEvent = 'Nibbles base', wledPs = 0, wledPresetNames = {}, finFollow = true, finPreset = 0;
let baseLightsMw = 0, baseBumping = 0, finProj = '';
const baseKeyEls = {};

// ------------------------------------------------------------------ keypad
function baseBuildKeypad() {
  const pad = $('basekeys');
  for (const col of BASE_COLS) {
    const c = document.createElement('div');
    c.className = 'kcol';
    c.innerHTML = `<div class="ktitle">${col.title}</div>`;
    for (const [name, label] of col.keys) {
      const k = document.createElement('div');
      k.className = 'key';
      k.innerHTML = `<span class="led"></span><span class="kl">${label}</span>`;
      const up = () => { if (k.classList.contains('down')) { k.classList.remove('down'); baseKey(name, false); } };
      k.addEventListener('pointerdown', (e) => { k.setPointerCapture(e.pointerId); k.classList.add('down'); baseKey(name, true); });
      k.addEventListener('pointerup', up);
      k.addEventListener('pointercancel', up);
      baseKeyEls[name] = k;
      c.appendChild(k);
    }
    pad.appendChild(c);
  }
  // Only a field you type into keeps the keys; a slider, checkbox or menu that
  // was just used still has focus, but the keys go to the base.
  const typingIn = (el) => el && (el.tagName === 'TEXTAREA' || el.isContentEditable ||
    (el.tagName === 'INPUT' && !['range', 'checkbox', 'radio', 'button'].includes(el.type)));
  // Physical key -> the base key its press started: the release ends that one,
  // even if Shift changed the character in between.
  const held = new Map();
  const keyFor = (e) => BASE_NUMPAD[e.code] || BASE_KEYS_TYPED[e.key.toLowerCase()];
  window.addEventListener('keydown', (e) => {
    if (e.repeat || held.has(e.code) || e.metaKey || e.ctrlKey || e.altKey) return;
    const name = keyFor(e);
    if (!name || !$('basekb').checked || typingIn(e.target)) return;
    e.preventDefault();
    held.set(e.code, name);
    baseKeyEls[name].classList.add('down');
    baseKey(name, true);
  });
  window.addEventListener('keyup', (e) => {
    const name = held.get(e.code);
    if (!name) return;
    held.delete(e.code);
    baseKeyEls[name].classList.remove('down');
    baseKey(name, false);
  });
  window.addEventListener('blur', () => {   // switching away mid-press: let everything go
    for (const name of held.values()) { baseKeyEls[name].classList.remove('down'); baseKey(name, false); }
    held.clear();
  });
}

function baseKey(name, down) {
  if (!ws || ws.readyState !== 1) return;
  if (down && name !== 'unlock' && !realMode && baseState && baseState.locked) {   // a press the lock swallows: say so
    const b = $('baselock');
    b.style.background = '#c0501a';
    setTimeout(() => { b.style.background = '#5a2a10'; }, 250);
  }
  if (realMode) ws.send(JSON.stringify({ realbase: { key: name, down } }));   // the real base, on USB
  else ws.send(JSON.stringify({ base: { key: name, down } }));
}

function baseHello() { if (ws && ws.readyState === 1) ws.send(JSON.stringify({ base: { hello: 1 } })); }

// ------------------------------------------------------------------ Live: what the page's WLED and eyes show
function baseEyePreset() {
  if (!eyesReady) return 0;
  return (sim.HEAPU32[sim._eyes_status() >> 2] | 0) + 1;
}
function baseEyeName(n) { return eyesReady && n > 0 && n <= sim._eyes_preset_count() ? sim.UTF8ToString(sim._eyes_preset_name(n - 1)) : ''; }

// WLED's layer slots as last reported ({"nibbles":{"slots":[...]}} in its state, or the shark's mirror)
let wledSlots = null;
function baseLayers() {
  const sl = (i) => wledSlots && wledSlots.find((x) => x.id === i);
  if (!wledSlots) return [-1, -1, -1, -1];
  const pal = (i) => (sl(i) && sl(i).on ? sl(i).pal : -1), layer = (i) => (sl(i) && sl(i).on ? sl(i).look || 0 : 0);
  return [pal(SLOT.BODY), pal(SLOT.FINS), layer(SLOT.BODY_OVER), layer(SLOT.FINS_OVER)];
}

setInterval(() => {
  if (!ws || ws.readyState !== 1) return;
  ws.send(JSON.stringify({ base: { live: [wledPs > 0 ? wledPs : 0, baseEyePreset(), finPreset, +$('bri').value, finFollow ? 1 : 0],
                                   layers: baseLayers() } }));
  if (!finFollow && lightsFrom !== 'shark') finsFromShark({ finfollow: false });   // the bench has no usermod: lock them here
}, 250);

// The WLED preset and fin following, from app.js's polling or the live shark's mirror.
// WLED's last applied preset: a fin preset is the fins', anything else the body's (a bump's isn't Live).
function baseWledState(state) {
  if (state && state.nibbles && state.nibbles.slots) wledSlots = state.nibbles.slots;
  if (!state || !('ps' in state) || bumpSaved.on || finBumpSaved) return;
  if (isFinPreset(state.ps)) finPreset = state.ps - FIN_BASE;
  else if (state.ps > 0) wledPs = state.ps;
}
function baseFromShark(m) {
  if (m.wled && 'ps' in m.wled && !bumpSaved.on) wledPs = m.wled.ps;   // the usermod reports body and fins apart
  if (m.wled && 'finps' in m.wled && !finBumpSaved) finPreset = m.wled.finps;
  if (m.wled && 'finproj' in m.wled) finProj = m.wled.finproj ? 'top' : 'side';
  if ('finfollow' in m) finFollow = !!m.finfollow;
  if (m.wled && m.wled.slots)   // [on, look, fx, palette, blend, opacity] per slot
    wledSlots = m.wled.slots.map((x, id) => ({ id, on: !!x[0], look: x[1], fx: x[2], pal: x[3], bm: x[4], o: x[5] }));
}
function baseWledPresets(presets) {
  wledPresetNames = {};
  for (const [id, p] of Object.entries(presets)) if (+id > 0 && p.n) wledPresetNames[+id] = p.n;
}

// The lights' power, estimated from the LED frame (WLED's 17 mA per LED at full white).
function baseFrame(raw) {
  let sum = 0;
  for (let i = 0; i < raw.length; i++) sum += raw[i];
  const ma5v = sum / 255 * 17 / 3;
  baseLightsMw = ma5v * 5 / 0.9;   // into the 90% converter
}

// ------------------------------------------------------------------ actions
let bumpSaved = { on: false };
async function wledBump(action, arg) {
  if (!bumpSaved.on) Object.assign(bumpSaved, { on: true, ps: wledPs, lit: wledOn, bri: +$('bri').value });
  baseBumping = action;
  const s = action === 1 ? { on: true, bri: 255, tt: 0, seg: { fx: 0, col: [[255, 255, 255]] } }
          : action === 2 ? { on: false, tt: 0 } : { ps: arg, tt: 0 };
  await wledPost(s).catch(() => {});
}
// Fin bumps: a fin preset while held, then the fins' own again.
let finBumpSaved = null;
async function finBump(n) {
  if (finBumpSaved === null) finBumpSaved = finPreset;
  await wledPost({ ps: FIN_BASE + n, tt: 0 }).catch(() => {});
}
async function finBumpEnd() {
  if (finBumpSaved === null) return;
  const back = finBumpSaved;
  finBumpSaved = null;
  await wledPost({ ps: FIN_BASE + back, tt: 0 }).catch(() => {});
}
async function wledBumpEnd() {
  if (!bumpSaved.on) return;
  bumpSaved.on = false;
  baseBumping = 0;
  if (bumpSaved.ps > 0) { wledPs = bumpSaved.ps; await wledPost({ ps: bumpSaved.ps, tt: 0 }).catch(() => {}); }
  await wledPost({ on: bumpSaved.lit, bri: bumpSaved.bri, tt: 0 }).catch(() => {});
}

// Palette, layer and scene bumps: the slots show it while held, then go back to
// what they were (or to what a slot command for one meanwhile said: that's
// LATCH), as the usermod does. A scene (TAKE's preview of a cued palette or
// layer look) may also carry a preset, bumped the usual way.
const slotBumps = {};   // by the base's bump target (TGT.BODY, TGT.FINS): { restores: {slot: request}, preset, newPreset }
const BUMP_SCENE = 6;
function slotPost(req) { return wledPost({ nibbles: { slot: req } }).catch(() => {}); }
function slotNow(s) {
  const cur = wledSlots && wledSlots.find((x) => x.id === s);
  return { pal: { id: s, pal: cur ? cur.pal : 0 }, look: cur && cur.on && cur.look ? { id: s, look: cur.look, on: true } : { id: s, on: false } };
}
function lookReq(s, v) { return v ? { id: s, look: v, on: true, tt: 0 } : { id: s, on: false, tt: 0 }; }
function slotBump(target, bump, arg) {
  const s = arg >> 8, v = arg & 255;
  slotBumps[target] = { restores: { [s]: bump === 4 ? slotNow(s).pal : slotNow(s).look } };
  baseBumping = bump;
  slotPost(bump === 4 ? { id: s, pal: v, tt: 0 } : lookReq(s, v));
}
async function sceneBump(target, arg, extra) {
  const fins = target === TGT.FINS, ps = arg & 255, layer = (arg >> 8) & 127;
  const palSlot = fins ? SLOT.FINS : SLOT.BODY, laySlot = fins ? SLOT.FINS_OVER : SLOT.BODY_OVER;
  const restores = {};
  if (extra) restores[palSlot] = slotNow(palSlot).pal;
  if (layer) restores[laySlot] = slotNow(laySlot).look;
  slotBumps[target] = { restores, preset: !!ps };
  baseBumping = BUMP_SCENE;
  if (ps && fins) await finBump(ps - FIN_BASE);
  else if (ps) await wledBump(3, ps);
  if (extra) await slotPost({ id: palSlot, pal: extra - 1, tt: 0 });
  if (layer) await slotPost(lookReq(laySlot, layer === 127 ? 0 : LAYER_BASE + layer));
}
async function slotBumpEnd(target) {
  const b = slotBumps[target];
  if (!b) return false;
  delete slotBumps[target];
  baseBumping = 0;
  if (b.preset && target === TGT.FINS) await finBumpEnd();
  else if (b.preset) await wledBumpEnd();
  // A new preset (LATCH) brings its own palette and layers: only commands go on top of it.
  for (const r of Object.values(b.restores))
    if (!b.newPreset || r.cmd) { const { cmd, ...req } = r; await slotPost({ ...req, tt: 0 }); }
  return true;
}
function slotCommand(slot, req) {
  const under = Object.values(slotBumps).find((b) => slot in b.restores);
  if (under) under.restores[slot] = { ...req, cmd: true };   // what the slot goes back to when the bump ends
  else slotPost(req);
}

function baseActions(list) {
  for (const [type, target, bump, value, extra] of list) {
    if (type === ACT.SLOT_LOOK) {
      slotCommand(target, value ? { id: target, look: value, on: true } : { id: target, on: false });
      baseEvent = value ? `layer ${value - LAYER_BASE} on ${target === SLOT.FINS_OVER ? 'the fins' : 'the body'}` : 'layer off';
      continue;
    }
    if (type === ACT.SLOT_PALETTE) {
      slotCommand(target, { id: target, pal: value });
      baseEvent = `palette ${value} on ${target === SLOT.FINS ? 'the fins' : 'the body'}`;
      continue;
    }
    if (type === ACT.BUMP && (bump === 4 || bump === 5)) { slotBump(target, bump, value); continue; }
    if (type === ACT.BUMP && bump === BUMP_SCENE) { sceneBump(target, value, extra); continue; }
    if (type === ACT.PRESET && slotBumps[target]) slotBumps[target].newPreset = true;
    if (type === ACT.BUMP_END && slotBumps[target]) { slotBumpEnd(target); continue; }
    if (type === ACT.PRESET && target === TGT.FINS) {
      if (finBumpSaved !== null) finBumpSaved = value;   // under a fin bump: its release lands here
      else wledPost({ ps: FIN_BASE + value }).catch(() => {});
      finPreset = value;
      baseEvent = value ? `fins preset ${value}` : 'fins follow the body';
    } else if (type === ACT.PRESET && target === TGT.BODY) {
      wledPost({ ps: value }).catch(() => {});
      wledPs = value;
      // ALL (bump = 1): the preset's eye settings too, a moment later, as they come by radio after the eyes' own command
      const e = bump && wledPresets[value] && wledPresets[value].nibbles && wledPresets[value].nibbles.eyes;
      if (e && lightsFrom !== 'shark') setTimeout(() => eyesApplySettings(e, value), 150);
      if (bumpSaved.on) bumpSaved.ps = value;   // a TAKE under its preview bump: the release keeps the new preset
      baseEvent = `wled set preset ${value}`;
    } else if (type === ACT.PRESET) {
      if (eyesReady) sim._eyes_command(NL.PRESET_SET, value - 1);
      baseEvent = `eyes set preset ${value}`;
    } else if (type === ACT.FINS) {
      finFollow = value === 2 ? !finFollow : !!value;
      if (lightsFrom === 'shark') wledPost({ nibbles: { finfollow: finFollow } }).catch(() => {});
      baseEvent = `fins ${finFollow ? 'follow' : 'locked'}`;
    } else if (type === ACT.BRIGHTNESS) {
      $('bri').value = value;
      $('bri').oninput();
      baseEvent = `brightness ${Math.round(value * 100 / 255)}%`;
    } else if (type === ACT.BUMP && target === TGT.FINS) {
      finBump(value);
    } else if (type === ACT.BUMP_END && target === TGT.FINS) {
      finBumpEnd();
    } else if (type === ACT.BUMP && target === TGT.BODY) {
      wledBump(bump, value);
    } else if (type === ACT.BUMP) {
      if (eyesReady) sim._eyes_bump(bump, value);
    } else if (type === ACT.BUMP_END && target === TGT.BODY) {
      wledBumpEnd();
    } else if (type === ACT.BUMP_END) {
      if (eyesReady) sim._eyes_bump(0, 0);
    }
  }
}

function baseMessage(m) {
  if (m.actions && m.actions.length) baseActions(m.actions);
  if (m.live) baseState = m;
  baseDraw();
}

// ------------------------------------------------------------------ screen (display.c's main page)
function preset(n, name) { return n <= 0 ? '--' : name ? `${n} ${name}` : `Preset ${n}`; }

// ------------------------------------------------------------------ the real base, on USB
// The keys go to the real base's serial console (server.py), so it sends real
// commands over the radio; its screen (a screenshot about once a second) and
// key LEDs show here.
let realMode = false;
const KEY_ORDER = ['1', '2', '3', '4', '5', '6', '7', '8', 'all', 'body', 'eyes', 'fins', 'page', 'latch', 'blackout',
  'finsensor', 'take', 'flash', 'unlock'];   // ctl_key_t order, as the base prints its LEDs
function setKeyLed(name, hex) {
  const el = baseKeyEls[name];
  if (!el) return;
  const c = '#' + hex;
  el.querySelector('.led').style.background = c;
  el.querySelector('.led').style.boxShadow = hex === '000000' ? 'none' : `0 0 10px ${c}`;
}
function realBaseMessage(m) {
  if ('connected' in m) {
    $('basedrivev').textContent = realMode ? (m.connected ? '(connected)' : '(not found on USB)') : '';
    if (realMode && !m.connected) { realMode = false; $('basedrive').value = 'sim'; $('basereal').hidden = true; }
  }
  if (m.leds && realMode) m.leds.forEach((hex, i) => setKeyLed(KEY_ORDER[i], hex));
  if (m.shot && realMode) {
    const bin = atob(m.shot), c = $('basereal'), g = c.getContext('2d');
    c.width = m.w; c.height = m.h;
    const img = g.createImageData(m.w, m.h);
    let p = 0;
    const put = (v) => {   // one RGB565 pixel
      img.data[p] = (v >> 11 & 31) * 255 / 31; img.data[p + 1] = (v >> 5 & 63) * 255 / 63; img.data[p + 2] = (v & 31) * 255 / 31; img.data[p + 3] = 255;
      p += 4;
    };
    if (m.rle) {   // runs: count - 1, then the pixel, little-endian
      for (let i = 0; i + 2 < bin.length && p < img.data.length; i += 3) {
        const v = bin.charCodeAt(i + 1) | bin.charCodeAt(i + 2) << 8;
        for (let r = bin.charCodeAt(i); r >= 0 && p < img.data.length; r--) put(v);
      }
      if (p !== img.data.length) return;   // cut short: keep the last whole one
    } else {
      for (let i = 0; i + 1 < bin.length && p < img.data.length; i += 2) put(bin.charCodeAt(i) | bin.charCodeAt(i + 1) << 8);   // RGB565, little-endian
    }
    g.putImageData(img, 0, 0);
    c.hidden = false;
  }
}
$('basedrive').addEventListener('change', () => {
  realMode = $('basedrive').value === 'real';
  $('basereal').hidden = true;
  $('basedrivev').textContent = realMode ? '(connecting…)' : '';
  if (ws && ws.readyState === 1) ws.send(JSON.stringify({ realbase: { on: realMode } }));
  baseDraw();
});

function baseDraw() {
  const s = baseState;
  // the WLED card's fin preset picker follows the fins
  if (document.activeElement !== $('finpreset')) $('finpreset').value = String(FIN_BASE + finPreset);
  const pr = wledPresets[FIN_BASE + finPreset];
  const proj = finProj || (finPreset && pr && pr.nibbles && pr.nibbles.finproj) || (finPreset ? 'side' : '');
  $('finprojv').textContent = finPreset ? `(${proj} view)` : '';
  // keys' LEDs (the real base's come in realBaseMessage)
  if (s && !realMode) for (const [name, el] of Object.entries(baseKeyEls)) {
    const c = s.leds[name] || '#000000';
    el.querySelector('.led').style.background = c;
    el.querySelector('.led').style.boxShadow = c === '#000000' ? 'none' : `0 0 10px ${c}`;
  }
  // POWER (simulated: the pack slider, the dock box and the lights' frame)
  const pct = +$('basepack').value, input = $('basein').value, docked = input !== 'none';
  const inPct = +$('baseinpct').value;
  const lightsW = baseLightsMw / 1000;
  // the LEMO input, left: an external battery's charge, or a USB-C supply's voltage (as the firmware)
  const setBatt = (pre, p, charging) => {
    $(pre + 'bar').style.width = `${p > 0 ? 1 + Math.floor(22 * p / 100) : 0}px`;
    $(pre + 'bar').style.background = charging ? '#34c759' : p <= 20 ? '#ff3b30' : '#fff';   // iOS green / red / white
    $(pre + 'bolt').style.visibility = charging ? 'visible' : 'hidden';
    $(pre + 'batt').style.borderColor = $(pre + 'nub').style.background = p < 0 ? 'rgba(255,255,255,.2)' : 'rgba(255,255,255,.4)';
  };
  const usb = input === 'usb';
  $('bs_inbatt').style.display = $('bs_innub').style.display = usb ? 'none' : '';
  setBatt('bs_in', input === 'battery' ? inPct : -1, false);
  $('bs_inpct').textContent = input === 'battery' ? `${inPct}%` : '';   // nothing plugged in: just the dim battery
  $('bs_inpct').style.color = input === 'battery' ? 'var(--text)' : 'var(--dim)';
  $('bs_inusb').textContent = usb ? 'USB 20.1 V' : '';   // the firmware draws the USB symbol
  $('baseinpctv').textContent = `${inPct}%`;
  // the battery icon: filled to the charge, a bolt while charging (as the firmware)
  const charging = docked && pct < 100 && !(input === 'battery' && inPct < 15);
  $('bs_pct').textContent = `${pct}%`;
  setBatt('bs_', pct, charging);
  const cellMv = 3000 + pct * 12, packV = cellMv * 4 / 1000;
  const mins = Math.round(pct / 100 * 80 * 60 / Math.max(lightsW + 2, 0.5));
  // the LEMO's current: the charger's share (to the pack) and the light bus's (lights + base), as power_flow()
  const inMv = !docked ? 0 : usb ? 20100 : 12000 + inPct * 48;
  const lightsA = lightsW / (docked ? inMv / 1000 : packV), baseA = 2 / (docked ? inMv / 1000 : packV);
  const starved = input === 'battery' && inPct < 15 && pct < 100;   // a flat external battery: the pack gets nothing
  const toPack = !docked || pct >= 100 || starved ? 0 : 2.0;
  const inA = docked ? toPack + lightsA + baseA : 0;
  const packA = !docked ? -(lightsA + baseA) : starved ? -(lightsA + baseA) : toPack * 0.92;
  const fig = (n, width) => String(n).padStart(width, '\u2007');   // figure spaces, as the firmware
  const tenths = (x, width) => `${fig(Math.floor(Math.round(x * 10) / 10), width)}.${Math.round(x * 10) % 10}`;
  // row 2: the LEMO's volts and amps, or with nothing plugged in the time left; the lights' amps and watts
  const hhmm = `${String(Math.floor(Math.min(mins, 5999) / 60)).padStart(2, '0')}H${String(Math.min(mins, 5999) % 60).padStart(2, '0')}M`;
  $('bs_inva').textContent = docked ? `${tenths(inMv / 1000, 2)}V ${tenths(inA, 1)}A` : hhmm;
  $('bs_inva').style.color = docked || mins > 45 ? 'var(--text)' : 'var(--warn)';
  $('bs_outwa').textContent = `${tenths(lightsA, 1)}A ${fig(Math.round(lightsW), 3)}W`;
  // row 3: cells / charger temperatures (simulated: steady), the pack's voltage
  const tempHtml = (c, warn, bad) => `<span style="color:${c >= bad ? '#ff3b30;font-weight:700' : c >= warn ? 'var(--warn)' : 'var(--dim)'}">${fig(c, 2)}°C</span>`;
  $('bs_temps').innerHTML = `${tempHtml(29, 45, 55)} / ${tempHtml(41, 85, 105)}`;
  $('bs_packv').textContent = `${packV.toFixed(2)}V`;
  const split = docked ? toPack / inA : -1, W = 148;
  const fi = $('bs_flowin'), fo = $('bs_flowout');
  if (split < 0 || starved) {   // one grey line: nothing in (dim), or starved (pulsing)
    fi.style.width = `${W}px`; fi.style.background = starved ? 'var(--dim)' : '#4a505a'; fo.style.display = 'none';
    fi.style.animation = starved ? 'bspulse .6s ease-in-out infinite alternate' : '';
  } else {
    const w = Math.round(W * split);
    fi.style.width = `${w}px`; fi.style.background = '#34c759'; fi.style.animation = '';
    fo.style.display = ''; fo.style.left = `${8 + w}px`; fo.style.width = `${W - w}px`; fo.style.background = '#0a84ff';
  }
  if (!s) return;
  // LIVE and STANDBY
  const rows = (pre, sc, bri, diff) => {
    $(pre + '0').textContent = preset(sc[0], wledPresetNames[sc[0]]);
    $(pre + '1').textContent = preset(sc[1], baseEyeName(sc[1]));
    $(pre + '2').textContent = sc[2] < 0 ? '--' : sc[2] ? preset(sc[2], wledPresetNames[FIN_BASE + sc[2]]) : 'body';
    for (let i = 0; i < 3; i++) $(pre + i).style.color = diff === null || diff & (1 << (i + 1)) ? 'var(--text)' : '#4a505a';
    if (bri !== null) $(pre + '3').textContent = `${Math.round(bri * 100 / 255)}%`;
  };
  rows('bs_l', s.live, +$('bri').value, null);
  // LOOK: the body's palette and the layer over it
  const look = (l, name) => {
    const [bp, , bl] = l || [-1, -1, 0, 0];
    const pal = bp < 0 ? '--' : name || `pal ${bp}`;
    return bl > 0 ? `${pal} + L${bl - LAYER_BASE}` : pal;
  };
  $('bs_l4').textContent = look(s.layers, s.pal_name);
  rows('bs_s', s.standby, null, s.diff);
  $('bs_s3').textContent = look(s.stby_layers, s.stby_pal_name);
  const lookDiff = s.stby_layers && s.layers && (s.stby_layers[0] !== s.layers[0] || s.stby_layers[2] !== s.layers[2]);
  $('bs_s3').style.color = lookDiff ? 'var(--text)' : '#4a505a';
  $('bs_s4').textContent = s.diff ? 'hold TAKE' : 'same as Live';
  $('bs_s4').style.color = s.diff ? '#30e060' : '#4a505a';
  $('bs_preview').textContent = s.preview ? 'PREVIEW' : '';
  $('bs_stby').style.background = s.preview ? '#0c3a18' : 'var(--card)';
  // target box and the rest
  const tcol = { ALL: '#ffffff', BODY: '#ff7ad9', EYES: '#36c8ff', FINS: '#ff8000' }[s.target];
  $('bs_target').style.background = s.locked ? '#4a505a' : tcol;
  $('bs_tname').textContent = s.target;
  $('bs_page').textContent = s.page_name || (s.page ? '9-16' : '1-8');
  $('bs_shift').textContent = s.latch ? 'LATCH' : '';
  $('bs_lock').textContent = s.locked ? 'LOCKED: hold UNLOCK' : '';
  $('baselock').hidden = !s.locked || realMode;
  $('bs_links').textContent = `Eyes sim  WLED ${lightsFrom === 'shark' ? 'shark' : 'bench'}`;
  const bumping = baseBumping || (s.preview ? 3 : 0);
  $('bs_event').textContent = bumping ? `BUMP  ${BUMP_NAMES[bumping]}` : baseEvent;
  $('bs_event').style.color = bumping ? 'var(--warn)' : 'var(--dim)';
  $('basescreen').style.outline = bumping ? '4px solid var(--warn)' : 'none';
}
setInterval(baseDraw, 250);

for (const id of ['basepack', 'basein', 'baseinpct']) $(id).addEventListener('input', () => { $('basepackv').textContent = `${$('basepack').value}%`; baseDraw(); });
baseBuildKeypad();
{   // the STANDBY panel is TAKE by touch, as on the base's screen
  const p = $('bs_stby');
  let down = false;
  const up = () => { if (down) { down = false; baseKey('take', false); } };
  p.addEventListener('pointerdown', (e) => { p.setPointerCapture(e.pointerId); down = true; baseKey('take', true); });
  p.addEventListener('pointerup', up);
  p.addEventListener('pointercancel', up);
}
