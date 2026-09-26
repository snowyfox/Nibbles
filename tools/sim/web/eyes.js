// The eyes: the eye firmware's own code (built into sim.wasm, see
// src/eyes_wasm.c) run in the page on the same sound, drawn on two canvases.
// Uses app.js's globals ($, sim, outPtr, audioCtx).
'use strict';

const EYE_FPS = 29.6;                    // the real eyes' frame rate
const NL = { PRESET_SET: 1, NEXT: 2, PREV: 3, AUTO: 6, REACT: 7, FLASH: 1, BLACKOUT: 2 };
let eyesReady = false, eyeImages = [], eyeLast = 0, eyeAcc = 0;
let eyeMic = { pending: new Float32Array(0), pos: 0, fill: 0 };

function eyesStart() {
  if (!sim._eyes_init((Math.random() * 2 ** 32) >>> 0)) { $('eyestatus').textContent = 'out of memory'; return; }
  const n = sim._eyes_size();
  eyeImages = ['eye_sb', 'eye_port'].map((id) => {
    const c = $(id);
    c.width = c.height = n;
    return c.getContext('2d').createImageData(n, n);
  });
  const sel = $('eyepreset');
  sel.innerHTML = '';
  for (let i = 0; i < sim._eyes_preset_count(); i++) {
    const o = document.createElement('option');
    o.value = i;
    o.textContent = `${i + 1}. ${sim.UTF8ToString(sim._eyes_preset_name(i))}`;
    sel.appendChild(o);
  }
  eyesReady = true;
  requestAnimationFrame(eyesTick);
}

// WLED's AudioReactive output for the block just processed (as the usermod reads it).
function eyesAR() {
  if (!eyesReady) return;
  const f = sim.HEAPF32, b = outPtr >> 2;  // ar_output_t: volume_smooth first, peak at +20, fft at +21
  sim._eyes_wled_audio(f[b], outPtr + 21, sim.HEAPU8[outPtr + 20]);
}

// Browser audio to the port eye's mic analysis: resampled, cut into frames.
function eyesFeed(chunk) {
  if (!eyesReady) return;
  const m = eyeMic;
  const merged = new Float32Array(m.pending.length + chunk.length);
  merged.set(m.pending);
  merged.set(chunk, m.pending.length);
  m.pending = merged;
  const rate = sim._eyes_mic_rate(), frame = sim._eyes_mic_frame(), buf = sim._eyes_mic_buffer() >> 2;
  const step = audioCtx.sampleRate / rate, gain = +$('gain').value;
  while (m.pos + 1 < m.pending.length) {
    const i = Math.floor(m.pos), fr = m.pos - i;
    sim.HEAPF32[buf + m.fill++] = (m.pending[i] * (1 - fr) + m.pending[i + 1] * fr) * gain;
    m.pos += step;
    if (m.fill === frame) { m.fill = 0; sim._eyes_mic_process(); }
  }
  const keep = Math.floor(m.pos);
  m.pending = m.pending.slice(keep);
  m.pos -= keep;
}

function eyesTick(t) {
  requestAnimationFrame(eyesTick);
  const dt = eyeLast ? (t - eyeLast) / 1000 : 0;
  eyeLast = t;
  eyeAcc = Math.min(eyeAcc + dt, 0.2);
  if (eyeAcc < 1 / EYE_FPS) return;
  const step = eyeAcc;
  eyeAcc = 0;
  sim._eyes_frame(step);
  const n = sim._eyes_size();
  ['eye_sb', 'eye_port'].forEach((id, k) => {
    const p = sim._eyes_image(k);
    eyeImages[k].data.set(sim.HEAPU8.subarray(p, p + n * n * 4));
    $(id).getContext('2d').putImageData(eyeImages[k], 0, 0);
  });
  showEyeStatus();
}

function showEyeStatus() {
  // eyes_status_t: 7 ints, 5 floats, 2 uint32
  const b = sim._eyes_status() >> 2, F = sim.HEAPF32, U = sim.HEAPU32;
  const [preset, rendered, , state, auto, peaksNow] = [0, 1, 2, 3, 4, 5].map((k) => U[b + k] | 0);
  const [tempo, level, loud, hype, gain] = [7, 8, 9, 10, 11].map((k) => F[b + k]);
  const name = sim.UTF8ToString(sim._eyes_preset_name(rendered));
  const sel = $('eyepreset');
  if (document.activeElement !== sel) sel.value = preset;
  $('eyecycle').checked = !!auto;
  $('eyestatus').textContent =
    `${rendered + 1}. ${name} · ${sim.UTF8ToString(sim._eyes_state_name(state))} · ` +
    `${tempo ? tempo.toFixed(0) + ' bpm' : 'no tempo'} · ${level.toFixed(0)} dB · loud ${loud.toFixed(2)} · ` +
    `hype ${hype.toFixed(2)} · ${peaksNow ? 'peaks' : 'beats'}` +
    ($('eyesrc').value === '1' ? ` · mic gain ${gain.toFixed(0)} dB` : '');
}

// Controls
$('eyepreset').onchange = () => sim._eyes_command(NL.PRESET_SET, +$('eyepreset').value);
$('eyeprev').onclick = () => sim._eyes_command(NL.PREV, 0);
$('eyenext').onclick = () => sim._eyes_command(NL.NEXT, 0);
$('eyecycle').onchange = () => sim._eyes_command(NL.AUTO, $('eyecycle').checked ? 1 : 0);
$('eyereact').onchange = () => sim._eyes_command(NL.REACT, +$('eyereact').value);
$('eyesrc').onchange = () => sim._eyes_source(+$('eyesrc').value);
$('eyebri').onchange = () => sim._eyes_brightness(+$('eyebri').value);
for (const [id, action] of [['eyeflash', NL.FLASH], ['eyeblack', NL.BLACKOUT]]) {  // held like the base's pads
  const b = $(id);
  const up = () => { if (b.classList.contains('on')) { b.classList.remove('on'); sim._eyes_bump(0, 0); } };
  b.addEventListener('pointerdown', (e) => { b.setPointerCapture(e.pointerId); b.classList.add('on'); sim._eyes_bump(action, 0); });
  b.addEventListener('pointerup', up);
  b.addEventListener('pointercancel', up);
}
