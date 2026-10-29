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
const ACT = { PRESET: 0, FINS: 1, BRIGHTNESS: 2, BUMP: 3, BUMP_END: 4 };
const TGT = { ALL: 0, BODY: 1, EYES: 2, FINS: 3 };
const BUMP_NAMES = { 1: 'FLASH', 2: 'BLACKOUT', 3: 'PRESET' };

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

setInterval(() => {
  if (!ws || ws.readyState !== 1) return;
  ws.send(JSON.stringify({ base: { live: [wledPs > 0 ? wledPs : 0, baseEyePreset(), finPreset, +$('bri').value, finFollow ? 1 : 0] } }));
  if (!finFollow && lightsFrom !== 'shark') finsFromShark({ finfollow: false });   // the bench has no usermod: lock them here
}, 250);

// The WLED preset and fin following, from app.js's polling or the live shark's mirror.
// WLED's last applied preset: a fin preset is the fins', anything else the body's (a bump's isn't Live).
function baseWledState(state) {
  if (!state || !('ps' in state) || bumpSaved.on || finBumpSaved) return;
  if (isFinPreset(state.ps)) finPreset = state.ps - FIN_BASE;
  else if (state.ps > 0) wledPs = state.ps;
}
function baseFromShark(m) {
  if (m.wled && 'ps' in m.wled && !bumpSaved.on) wledPs = m.wled.ps;   // the usermod reports body and fins apart
  if (m.wled && 'finps' in m.wled && !finBumpSaved) finPreset = m.wled.finps;
  if (m.wled && 'finproj' in m.wled) finProj = m.wled.finproj ? 'top' : 'side';
  if ('finfollow' in m) finFollow = !!m.finfollow;
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

function baseActions(list) {
  for (const [type, target, bump, value] of list) {
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
  const pct = +$('basepack').value, docked = $('basedock').checked;
  const lightsW = baseLightsMw / 1000;
  $('bs_source').textContent = docked ? 'DOCK' : 'BATTERY';
  $('bs_source').style.color = docked ? 'var(--ok)' : 'var(--warn)';
  $('bs_pct').textContent = `${pct}%`;
  const cellMv = 3000 + pct * 12;
  $('bs_volts').textContent = `${(cellMv * 4 / 1000).toFixed(2)} V`;
  $('bs_amps').textContent = docked ? '1.8 A' : `-${(lightsW / (cellMv * 4 / 1000)).toFixed(1)} A`;
  $('bs_bar').style.width = `${pct}%`;
  $('bs_bar').style.background = pct > 40 ? 'var(--ok)' : pct > 15 ? 'var(--warn)' : '#ff4d4d';
  const mins = Math.round(pct / 100 * 80 * 60 / Math.max(lightsW + 2, 0.5));
  $('bs_line1').textContent = docked ? (pct >= 100 ? 'Dock: pack full' : 'Charging 1.8 A') : `${Math.floor(mins / 60)} h ${String(mins % 60).padStart(2, '0')} min left`;
  $('bs_line1').style.color = docked ? 'var(--ok)' : mins > 45 ? 'var(--text)' : 'var(--warn)';
  $('bs_line2').textContent = `Lights ${Math.round(lightsW)} W  ${(lightsW / (cellMv * 4 / 1000)).toFixed(1)} A`;
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
  rows('bs_s', s.standby, null, s.diff);
  $('bs_s3').textContent = s.diff ? 'hold TAKE' : 'same as Live';
  $('bs_s3').style.color = s.diff ? '#30e060' : '#4a505a';
  $('bs_preview').textContent = s.preview ? 'PREVIEW' : '';
  $('bs_stby').style.background = s.preview ? '#0c3a18' : 'var(--card)';
  // target box and the rest
  const tcol = { ALL: '#ffffff', BODY: '#ff7ad9', EYES: '#36c8ff', FINS: '#ff8000' }[s.target];
  $('bs_target').style.background = s.locked ? '#4a505a' : tcol;
  $('bs_tname').textContent = s.target;
  $('bs_page').textContent = s.page ? '9-16' : '1-8';
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

for (const id of ['basepack', 'basedock']) $(id).addEventListener('input', () => { $('basepackv').textContent = `${$('basepack').value}%`; baseDraw(); });
baseBuildKeypad();
{   // the STANDBY panel is TAKE by touch, as on the base's screen
  const p = $('bs_stby');
  let down = false;
  const up = () => { if (down) { down = false; baseKey('take', false); } };
  p.addEventListener('pointerdown', (e) => { p.setPointerCapture(e.pointerId); down = true; baseKey('take', true); });
  p.addEventListener('pointerup', up);
  p.addEventListener('pointercancel', up);
}
