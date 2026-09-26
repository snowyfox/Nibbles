// 3D view of the shark: the Fusion 360 mesh as a dark, see-through body with
// every LED drawn as a glowing point at its real position.
// Used by app.js through window.nibbles3d.
import * as THREE from 'three';
import { OrbitControls } from './vendor/OrbitControls.js';
import { OBJLoader } from './vendor/OBJLoader.js';

let renderer, scene, camera, controls, container, home;
let core, glow, colors, count = 0, shown = false, hidden = null;
let rgbNow = null, offPlane = [], leds3 = null;  // latest WLED frame; LEDs off the body's plane
const eyeTextures = [];
const CORE_SIZE = 6, GLOW_SIZE = 22;  // mm, times the page's LED size setting
const EYE_BACK_DIM = 0.4;  // an eye screen seen from behind: this bright

// Soft round dot, drawn once, used for both the glow and the bright core.
function dotTexture() {
  const c = document.createElement('canvas');
  c.width = c.height = 64;
  const g = c.getContext('2d');
  const grad = g.createRadialGradient(32, 32, 0, 32, 32, 32);
  grad.addColorStop(0, 'rgba(255,255,255,1)');
  grad.addColorStop(0.35, 'rgba(255,255,255,0.55)');
  grad.addColorStop(1, 'rgba(255,255,255,0)');
  g.fillStyle = grad;
  g.fillRect(0, 0, 64, 64);
  return new THREE.CanvasTexture(c);
}

function build(layout) {
  container = document.getElementById('view3d');
  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio || 1);
  container.appendChild(renderer.domElement);
  scene = new THREE.Scene();
  scene.background = new THREE.Color(0x000000);
  camera = new THREE.PerspectiveCamera(40, 1, 5, 20000);
  controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true;
  scene.add(new THREE.HemisphereLight(0x8090b0, 0x101018, 1.2));
  const sun = new THREE.DirectionalLight(0xffffff, 0.8);
  sun.position.set(300, 600, 400);
  scene.add(sun);

  // LEDs (layout.leds is already y-up, in mm)
  count = layout.leds.length;
  hidden = layout.hidden;
  leds3 = layout.leds;
  offPlane = [];
  layout.leds.forEach((p, i) => { if (Math.abs(p[0]) >= 4) offPlane.push(i); });
  const pos = new Float32Array(count * 3);
  layout.leds.forEach((p, i) => pos.set(p, i * 3));
  colors = new Float32Array(count * 3).fill(0.08);
  if (hidden) hidden.forEach((h, i) => { if (h) colors.fill(0, i * 3, i * 3 + 3); });
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.BufferAttribute(pos, 3));
  geo.setAttribute('color', new THREE.BufferAttribute(colors, 3));
  const tex = dotTexture();
  const mat = (size, opacity) => new THREE.PointsMaterial({
    size, map: tex, vertexColors: true, transparent: true, opacity,
    blending: THREE.AdditiveBlending, depthWrite: false, sizeAttenuation: true,
  });
  glow = new THREE.Points(geo, mat(GLOW_SIZE, 0.5));  // the diffuser's soft spread
  core = new THREE.Points(geo, mat(CORE_SIZE, 1.0));
  scene.add(glow, core);

  // Frame the LEDs
  geo.computeBoundingSphere();
  const { center, radius } = geo.boundingSphere;
  controls.target.copy(center);
  // Level with the shark (its XY plane edge-on), from the starboard front
  // quarter: design X points left and Y (the nose) right, both 45° towards
  // the viewer.
  camera.position.set(center.x + radius * 1.6, center.y, center.z - radius * 1.6);
  controls.update();
  home = { target: controls.target.clone(), position: camera.position.clone() };

  // The shark's body: Fusion mesh (Z up, cm) turned to y-up mm
  if (layout.mesh) {
    new OBJLoader().load(layout.mesh.file, (obj) => {
      obj.scale.setScalar(layout.mesh.scale || 1);
      if (layout.up === 'z') obj.rotation.x = -Math.PI / 2;
      const body = new THREE.MeshStandardMaterial({
        color: 0x39414f, roughness: 0.8, metalness: 0.0,
        transparent: true, opacity: 0.22, depthWrite: false, side: THREE.DoubleSide,
      });
      // Drawn before the LEDs whatever the angle (three.js otherwise orders
      // see-through objects by distance, and the body would darken LEDs in
      // front of it from some angles). LEDs behind it are dimmed in shadeLeds.
      obj.traverse((m) => { if (m.isMesh) { m.material = body; m.renderOrder = -1; } });
      scene.add(obj);
    });
  }
  // The eyes: each screen a disc showing its eye canvas (see eyes.js), in
  // front of a dark bezel.
  for (const e of layout.eyes || []) {
    const canvas = document.getElementById(e.side === 'port' ? 'eye_port' : 'eye_sb');
    const tex = new THREE.CanvasTexture(canvas);
    tex.colorSpace = THREE.SRGBColorSpace;
    eyeTextures.push(tex);
    const r = e.right.map(Number), n = e.normal.map(Number), u = e.up.map(Number);
    const place = (mesh, out) => {
      mesh.matrixAutoUpdate = false;
      mesh.matrix.makeBasis(new THREE.Vector3(...r), new THREE.Vector3(...u), new THREE.Vector3(...n))
        .setPosition(...e.centre.map((c, i) => c + n[i] * out));
      scene.add(mesh);
    };
    place(new THREE.Mesh(new THREE.CircleGeometry(e.radius_mm + 3, 48),
                         new THREE.MeshStandardMaterial({ color: 0x08090c, roughness: 0.4 })), -0.5);
    place(new THREE.Mesh(new THREE.CircleGeometry(e.radius_mm, 64),
                         new THREE.MeshBasicMaterial({ map: tex, toneMapped: false })), 0);
    // Seen from behind (through the body): the same picture, mirrored and dimmer.
    place(new THREE.Mesh(new THREE.CircleGeometry(e.radius_mm, 64),
                         new THREE.MeshBasicMaterial({ map: tex, toneMapped: false, side: THREE.BackSide,
                           color: new THREE.Color().setRGB(EYE_BACK_DIM, EYE_BACK_DIM, EYE_BACK_DIM, THREE.SRGBColorSpace) })), 0);
  }
  new ResizeObserver(resize).observe(container);
  resize();
}

function resize() {
  if (!renderer) return;
  const w = container.clientWidth, h = container.clientHeight;
  renderer.setSize(w, h, false);
  camera.aspect = w / h;
  camera.updateProjectionMatrix();
}

function frame() {
  if (!shown) return;
  controls.update();
  for (const t of eyeTextures) t.needsUpdate = true;  // the eyes redraw ~30 times a second
  shadeLeds();
  const k = window.nibblesLedScale || 1;  // the LED size slider
  core.material.size = CORE_SIZE * k;
  glow.material.size = GLOW_SIZE * k;
  renderer.render(scene, camera);
  requestAnimationFrame(frame);
}

// LED colours for this frame: hidden channels off, LEDs seen through the
// body (from where the camera is now) dimmed like app.js's canvas views.
const BODY_DIM = Math.pow(0.2, 2.2);  // 20% as seen (colours here are linear light)
function shadeLeds() {
  if (!colors || !rgbNow) return;
  const n = Math.min(count, rgbNow.length / 3);
  for (let i = 0; i < n * 3; i++) colors[i] = hidden && hidden[(i / 3) | 0] ? 0 : rgbNow[i] / 255;
  const behind = window.nibblesBehindBody, c = camera.position;
  if (behind) {
    for (const i of offPlane) {
      if (i >= n) continue;
      const p = leds3[i];
      if (behind(p, [c.x - p[0], c.y - p[1], c.z - p[2]])) {
        colors[i * 3] *= BODY_DIM; colors[i * 3 + 1] *= BODY_DIM; colors[i * 3 + 2] *= BODY_DIM;
      }
    }
  }
  core.geometry.attributes.color.needsUpdate = true;
}

window.nibbles3d = {
  show(layout) {
    if (!renderer) build(layout);
    shown = true;
    container.hidden = false;
    resize();
    requestAnimationFrame(frame);
  },
  // Look at point from dist mm, from direction dir (default: the starboard front
  // quarter); null = the whole shark.
  focus(point, dist, dir = [1, 0.25, -0.55]) {
    if (!controls) return;
    if (!point) {
      controls.target.copy(home.target);
      camera.position.copy(home.position);
    } else {
      controls.target.set(...point);
      camera.position.copy(controls.target).add(new THREE.Vector3(...dir).normalize().multiplyScalar(dist));
    }
    controls.update();
  },
  // A world direction in camera space: [right, up, towards the viewer].
  viewDir(v) {
    if (!camera) return null;
    const d = new THREE.Vector3(...v).transformDirection(camera.matrixWorldInverse);
    return [d.x, d.y, d.z];
  },
  hide() {
    shown = false;
    if (container) container.hidden = true;
  },
  // RGB bytes from WLED, one triple per LED (applied each frame, see shadeLeds)
  update(rgb) {
    rgbNow = rgb;
  },
};
