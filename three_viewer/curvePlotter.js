import * as THREE from "three";
import { ParametricGeometry } from "three/examples/jsm/geometries/ParametricGeometry.js";

const WIRE_COLOR = 0x4a8cff;

// Radial scale range — avoids the origin singularity (t=0 collapses all
// points to [0,0,0]) by starting slightly above zero. Two halves (t>0
// and t<0) cover both nappes of the cone.
const T_MIN = 0.02;
const T_MAX = 1.0;

function makeWireMaterial() {
  return new THREE.MeshBasicMaterial({
    color: WIRE_COLOR,
    wireframe: true,
  });
}

// Build the cone over a planar curve y = yFunc(x) in the Z=1 chart.
// Surface point: t * [x, yFunc(x), 1], so Z = t (not always 1).
// Two halves for t > 0 and t < 0 (both nappes of the cone).
function buildConeSurface(xMin, xMax, yFunc, resolution, material, tMin = 0.1, tMax = 5) {
  const slices = Math.max(10 * resolution, 10);
  const stacks = Math.max(4 * resolution, 4);
  const group = new THREE.Group();

  // Ensure double-sided material so inverted t-halves render properly
  material.side = THREE.DoubleSide;

  const makeHalf = (tLo, tHi) => {
    const fn = (u, v, target) => {
      const x = xMin + (xMax - xMin) * u;
      const y = yFunc(x);
      const t = tLo + (tHi - tLo) * v;
      target.set(t * x, t * y, t);
    };
    return new THREE.Mesh(new ParametricGeometry(fn, slices, stacks), material);
  };

  group.add(makeHalf(tMin, tMax));
  group.add(makeHalf(-tMax, -tMin));
  return group;
}

// Fermat homogeneous: X³ + Y³ = aZ³
// Z=1 chart: y = cbrt(a - x³)
function fermatSurface(a, resolution) {
  const xRange = 3 * Math.cbrt(Math.max(a, 1));
  const material = makeWireMaterial();
  return buildConeSurface(
    -xRange,
    xRange,
    (x) => Math.cbrt(a - x * x * x),
    resolution,
    material
  );
}

// Weierstrass homogeneous: Y²Z = X³ + aX²Z + bXZ² + cZ³
// Z=1 chart: y² = x³ + ax² + bx + c  (two branches ±y)
// Find intervals where the cubic RHS >= 0, build a cone over each.
function weierstrassSurface(a, b, c, resolution) {
  const xRange = Math.max(15, Math.cbrt(Math.abs(c)) * 2);
  const material = makeWireMaterial();
  const group = new THREE.Group();

  const N = 500;
  const intervals = [];
  let start = null;

  const f = (x) => x * x * x + a * x * x + b * x + c;

  // Root finding helper for clean endpoints
  const findRoot = (x1, x2) => {
    let mid;
    for (let i = 0; i < 10; i++) {
      mid = (x1 + x2) / 2;
      if (f(mid) * f(x1) <= 0) x2 = mid;
      else x1 = mid;
    }
    return mid;
  };

  for (let i = 0; i < N; i++) {
    const xCurr = -xRange + (2 * xRange * i) / N;
    const xNext = -xRange + (2 * xRange * (i + 1)) / N;
    const y1 = f(xCurr);
    const y2 = f(xNext);

    if (y1 >= 0 && start === null) {
      start = (i > 0 && f(-xRange + (2 * xRange * (i - 1)) / N) < 0) 
        ? findRoot(-xRange + (2 * xRange * (i - 1)) / N, xCurr) 
        : xCurr;
    }

    if (y1 >= 0 && y2 < 0 && start !== null) {
      const root = findRoot(xCurr, xNext);
      intervals.push([start, root]);
      start = null;
    }
  }
  if (start !== null) intervals.push([start, xRange]);

  for (const [xMin, xMax] of intervals) {
    const yFunc = (x) => Math.sqrt(Math.max(f(x), 0));
    group.add(buildConeSurface(xMin, xMax, yFunc, resolution, material, 0.1, 50));
    group.add(buildConeSurface(xMin, xMax, (x) => -yFunc(x), resolution, material));
  }

  return group;
}

// Scale the object so its bounding box max dimension = 1.
function normalizeToUnit(object) {
  const box = new THREE.Box3().setFromObject(object);
  if (box.isEmpty()) return;
  const size = new THREE.Vector3();
  box.getSize(size);
  const maxDim = Math.max(size.x, size.y, size.z) || 1;
  object.scale.setScalar(1 / maxDim);
}

export class CurvePlotter {
  constructor(scene) {
    this.scene = scene;
    this.curveObject = null;
    this.resolution = 2;
  }

  clear() {
    if (!this.curveObject) return;
    this.scene.remove(this.curveObject);
    this.curveObject.traverse((obj) => {
      if (obj.geometry) obj.geometry.dispose();
      if (obj.material) {
        const mats = Array.isArray(obj.material) ? obj.material : [obj.material];
        mats.forEach((m) => m.dispose());
      }
    });
    this.curveObject = null;
  }

  plot(form, params) {
    this.clear();

    let object;
    if (form === "fermat") {
      object = fermatSurface(params.a, this.resolution);
    } else if (form === "weierstrass") {
      object = weierstrassSurface(
        params.a,
        params.b,
        params.c,
        this.resolution
      );
    } else {
      return { ok: false, message: "Unknown curve form" };
    }

    normalizeToUnit(object);
    this.scene.add(object);
    this.curveObject = object;
    return { ok: true };
  }
}

export function setupCurveControls(app, {
  formSelect,
  fermatParams,
  weierstrassParams,
  plotBtn,
  statusEl,
}) {
  const plotter = new CurvePlotter(app.scene);

  const aInput = fermatParams.querySelector("#fermat-a");
  const weierAInput = weierstrassParams.querySelector("#weier-a");
  const weierBInput = weierstrassParams.querySelector("#weier-b");
  const weierCInput = weierstrassParams.querySelector("#weier-c");

  const updateFormVisibility = () => {
    const form = formSelect.value;
    fermatParams.hidden = form !== "fermat";
    weierstrassParams.hidden = form !== "weierstrass";
  };

  formSelect.addEventListener("change", updateFormVisibility);
  updateFormVisibility();

  plotBtn.addEventListener("click", () => {
    const form = formSelect.value;
    let params;
    if (form === "fermat") {
      const a = parseFloat(aInput.value);
      if (!Number.isFinite(a) || a <= 0) {
        statusEl.textContent = "a must be a number > 0";
        return;
      }
      params = { a };
    } else if (form === "weierstrass") {
      const a = parseFloat(weierAInput.value);
      const b = parseFloat(weierBInput.value);
      const c = parseFloat(weierCInput.value);
      if (!Number.isFinite(a) || !Number.isFinite(b) || !Number.isFinite(c)) {
        statusEl.textContent = "a, b, c must be numbers";
        return;
      }
      params = { a, b, c };
    } else {
      return;
    }

    const result = plotter.plot(form, params);
    if (result.ok) {
      statusEl.textContent = `Plotted ${formSelect.options[formSelect.selectedIndex].text}`;
    } else {
      statusEl.textContent = result.message;
    }
  });
}
