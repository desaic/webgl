import * as THREE from "three";
import { MeshBVH } from "three-mesh-bvh";
import { decimate } from "./cull";
import { Item } from "./scene";

const N_RIDGE_VERTS = 160;
const N_AREA_SAMPLES = 80;
const MAX_NUDGE_CM = 5;
const SIM_DECIMATE_CELLS = 16;
const simGeos = new WeakMap<THREE.BufferGeometry, THREE.BufferGeometry>();
const bvhs = new WeakMap<THREE.BufferGeometry, MeshBVH>();
const sampleCache = new WeakMap<THREE.BufferGeometry, THREE.Vector3[]>();

function simGeoOf(g: THREE.BufferGeometry): THREE.BufferGeometry {
  let sg = simGeos.get(g);
  if (!sg) {
    sg = decimate(g, SIM_DECIMATE_CELLS);
    simGeos.set(g, sg);
  }
  return sg;
}

function bvhOf(g: THREE.BufferGeometry): MeshBVH {
  let b = bvhs.get(g);
  if (!b) {
    b = new MeshBVH(g);
    bvhs.set(g, b);
  }
  return b;
}

function samplesOf(g: THREE.BufferGeometry): THREE.Vector3[] {
  let s = sampleCache.get(g);
  if (s) return s;
  const pos = g.attributes.position;
  if (!g.attributes.normal) g.computeVertexNormals();
  const norm = g.attributes.normal;
  const idx = g.index;
  const nt = (idx ? idx.count : pos.count) / 3;
  const vi = (t: number, k: number) => (idx ? idx.getX(t * 3 + k) : t * 3 + k);
  const a = new THREE.Vector3();
  const b = new THREE.Vector3();
  const c = new THREE.Vector3();
  const fn = new THREE.Vector3();
  const vn = new THREE.Vector3();
  const sharpness = new Float32Array(pos.count);
  const cum = new Float64Array(nt);
  let total = 0;
  for (let t = 0; t < nt; t++) {
    const i0 = vi(t, 0), i1 = vi(t, 1), i2 = vi(t, 2);
    a.fromBufferAttribute(pos, i0);
    b.fromBufferAttribute(pos, i1).sub(a);
    c.fromBufferAttribute(pos, i2).sub(a);
    fn.crossVectors(b, c);
    const area2 = fn.length();
    total += area2 * 0.5;
    cum[t] = total;
    if (area2 > 1e-10) {
      fn.divideScalar(area2);
      for (const vi0 of [i0, i1, i2]) {
        vn.fromBufferAttribute(norm, vi0);
        const curv = 1 - Math.abs(fn.dot(vn));
        if (curv > sharpness[vi0]) sharpness[vi0] = curv;
      }
    }
  }

  // 1. Prioritize sharpest ridge/fin vertices so thin fins are never missed
  const order = Array.from({ length: pos.count }, (_, i) => i);
  order.sort((i, j) => sharpness[j] - sharpness[i]);
  s = [];
  const nRidge = Math.min(pos.count, N_RIDGE_VERTS);
  for (let i = 0; i < nRidge; i++) {
    s.push(new THREE.Vector3().fromBufferAttribute(pos, order[i]));
  }

  // 2. Add area-weighted samples to cover flat regions
  let seed = 13374242;
  const rnd = () => ((seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0) / 4294967296);
  for (let i = 0; i < N_AREA_SAMPLES && total > 0; i++) {
    const target = rnd() * total;
    let lo = 0;
    let hi = nt - 1;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if (cum[mid] < target) lo = mid + 1;
      else hi = mid;
    }
    a.fromBufferAttribute(pos, vi(lo, 0));
    b.fromBufferAttribute(pos, vi(lo, 1));
    c.fromBufferAttribute(pos, vi(lo, 2));
    let u = rnd();
    let v = rnd();
    if (u + v > 1) {
      u = 1 - u;
      v = 1 - v;
    }
    s.push(a.clone().addScaledVector(b.clone().sub(a), u).addScaledVector(c.clone().sub(a), v));
  }
  sampleCache.set(g, s);
  return s;
}

const _ray = new THREE.Ray();
const RAY_DIRS = [
  new THREE.Vector3(1, 0.1234, 0.0567).normalize(),
  new THREE.Vector3(0.0789, 1, 0.1357).normalize(),
  new THREE.Vector3(0.0246, 0.0913, 1).normalize(),
];
function insideByRays(bvh: MeshBVH, p: THREE.Vector3): boolean {
  let votes = 0;
  for (const d of RAY_DIRS) {
    _ray.set(p, d);
    if (bvh.raycast(_ray, THREE.DoubleSide).length % 2 === 1) votes++;
  }
  return votes >= 2;
}

interface Body {
  item: Item;
  startPos: THREE.Vector3;
  pos: THREE.Vector3;
  quat: THREE.Quaternion;
  prevPos: THREE.Vector3;
  prevQuat: THREE.Quaternion;
  vel: THREE.Vector3;
  angVel: THREE.Vector3;
  invMass: number;
  invI: number;
  radius: number;
  scale: number;
  box: THREE.Box3;
}

interface StaticObstacle {
  item: Item;
  box: THREE.Box3;
  mat: THREE.Matrix4;
  inv: THREE.Matrix4;
  radius: number;
  scale: number;
}

export interface ForceSpec {
  item: Item;
  localPoint: THREE.Vector3;
  target: THREE.Vector3; // world target position to pull the attach point toward
  magnitude: number;
}

const _mat = new THREE.Matrix4();
const _inv = new THREE.Matrix4();
const _p = new THREE.Vector3();
const _wp = new THREE.Vector3();
const _wn = new THREE.Vector3();
const _ra = new THREE.Vector3();
const _c1 = new THREE.Vector3();
const _c2 = new THREE.Vector3();
const _dp = new THREE.Vector3();
const _dq = new THREE.Quaternion();
const _broadBox = new THREE.Box3();
const _hit = { point: new THREE.Vector3(), distance: 0, faceIndex: 0 };

function applyRotDelta(q: THREE.Quaternion, dw: THREE.Vector3) {
  const len = dw.length();
  if (len < 1e-8) return;
  _dq.setFromAxisAngle(dw.clone().divideScalar(len), len);
  q.premultiply(_dq).normalize();
}

export class RigidBodySim {
  active: Body | null = null;
  obstacles: StaticObstacle[] = [];
  potentialContacts: StaticObstacle[] = [];
  forces: (ForceSpec | null)[] = [null, null];
  running = false;
  stepsLeft = 0;
  prof = {
    steps: 0,
    initMs: 0,
    stepMs: 0,
    broadMs: 0,
    obsMs: 0,
    broadCount: 0,
    narrowPairs: 0,
  };

  private broadphasePos = new THREE.Vector3();
  private broadphaseMargin = 0;

  init(selected: Item | null, items: Item[]) {
    const t0 = performance.now();
    this.obstacles = [];
    this.potentialContacts = [];
    this.active = null;
    this.prof = { steps: 0, initMs: 0, stepMs: 0, broadMs: 0, obsMs: 0, broadCount: 0, narrowPairs: 0 };
    if (!selected || !selected.visible) return;

    const s = selected.mesh.scale.x;
    const r = Math.max(0.5, selected.radius * s);
    const m = Math.max(0.1, r * r * r * 0.01);
    this.active = {
      item: selected,
      startPos: selected.mesh.position.clone(),
      pos: selected.mesh.position.clone(),
      quat: selected.mesh.quaternion.clone(),
      prevPos: selected.mesh.position.clone(),
      prevQuat: selected.mesh.quaternion.clone(),
      vel: new THREE.Vector3(),
      angVel: new THREE.Vector3(),
      invMass: 1 / m,
      invI: 1 / (0.4 * m * r * r),
      radius: r,
      scale: s,
      box: new THREE.Box3(),
    };
    this.broadphaseMargin = r * 1.5;

    for (const it of items) {
      if (!it.visible || it === selected) continue;
      it.mesh.updateMatrixWorld(true);
      const os = it.mesh.scale.x;
      const or = Math.max(0.5, it.radius * os);
      const mat = it.mesh.matrixWorld.clone();
      const inv = mat.clone().invert();
      const box = new THREE.Box3().setFromObject(it.mesh);
      this.obstacles.push({ item: it, box, mat, inv, radius: or, scale: os });
    }

    this.updateBox(this.active);
    this.updateBroadphase();
    this.prof.initMs = performance.now() - t0;
  }

  private updateBox(b: Body) {
    b.box.min.set(b.pos.x - b.radius, b.pos.y - b.radius, b.pos.z - b.radius);
    b.box.max.set(b.pos.x + b.radius, b.pos.y + b.radius, b.pos.z + b.radius);
  }

  private updateBroadphase() {
    if (!this.active) return;
    this.broadphasePos.copy(this.active.pos);
    _broadBox.copy(this.active.box).expandByScalar(this.broadphaseMargin);
    this.potentialContacts = this.obstacles.filter((o) => _broadBox.intersectsBox(o.box));
  }

  private resolveObstacle(a: Body, obs: StaticObstacle) {
    const geoObs = simGeoOf(obs.item.full);
    const geoA = simGeoOf(a.item.full);
    const bvhObs = bvhOf(geoObs);
    const bvhA = bvhOf(geoA);
    _mat.compose(a.pos, a.quat, _c2.set(a.scale, a.scale, a.scale));
    _inv.copy(_mat).invert();

    let bestDepth = 0;
    const bestWp = new THREE.Vector3();
    const bestWn = new THREE.Vector3();

    // 1. active samples inside static obstacle
    for (const lp of samplesOf(geoA)) {
      _wp.copy(lp).applyMatrix4(_mat);
      if (!obs.box.containsPoint(_wp)) continue;
      _p.copy(_wp).applyMatrix4(obs.inv);
      const hit = bvhObs.closestPointToPoint(_p, _hit, 0, a.radius / obs.scale);
      if (!hit || !insideByRays(bvhObs, _p)) continue;
      const d = hit.distance * obs.scale;
      if (d > bestDepth) {
        bestDepth = d;
        bestWp.copy(_wp);
        // hit.point is on obs surface, _p is inside obs => hit.point - _p points out of obs
        bestWn.copy(hit.point).sub(_p);
        if (bestWn.lengthSq() < 1e-10) bestWn.copy(a.pos).sub(obs.item.mesh.position);
        else bestWn.transformDirection(obs.mat);
        bestWn.normalize();
      }
    }

    // 2. static obstacle samples inside active
    for (const lp of samplesOf(geoObs)) {
      _wp.copy(lp).applyMatrix4(obs.mat);
      if (!a.box.containsPoint(_wp)) continue;
      _p.copy(_wp).applyMatrix4(_inv);
      const hit = bvhA.closestPointToPoint(_p, _hit, 0, obs.radius / a.scale);
      if (!hit || !insideByRays(bvhA, _p)) continue;
      const d = hit.distance * a.scale;
      if (d > bestDepth) {
        bestDepth = d;
        bestWp.copy(_wp);
        // hit.point is on a's surface, _p is inside a => _p - hit.point pushes a away from obs
        bestWn.copy(_p).sub(hit.point);
        if (bestWn.lengthSq() < 1e-10) bestWn.copy(a.pos).sub(obs.item.mesh.position);
        else bestWn.transformDirection(_mat);
        bestWn.normalize();
      }
    }

    if (bestDepth <= 1e-4) return;
    _ra.copy(bestWp).sub(a.pos);
    _c1.crossVectors(_ra, bestWn);
    const wA = a.invMass + a.invI * _c1.lengthSq();
    if (wA < 1e-10) return;

    const lambda = bestDepth / wA;
    _dp.copy(bestWn).multiplyScalar(lambda);
    a.pos.addScaledVector(_dp, a.invMass);
    applyRotDelta(a.quat, _c1.crossVectors(_ra, _dp).multiplyScalar(a.invI));
  }

  step(dt = 1 / 60, substeps = 3, iters = 2) {
    const a = this.active;
    if (!a) return;
    const tStep = performance.now();
    this.prof.steps++;
    const sdt = dt / substeps;

    for (let s = 0; s < substeps; s++) {
      a.prevPos.copy(a.pos);
      a.prevQuat.copy(a.quat);

      for (const f of this.forces) {
        if (!f || f.item !== a.item) continue;
        _wp.copy(f.localPoint).multiplyScalar(a.scale).applyQuaternion(a.quat).add(a.pos);
        _ra.copy(_wp).sub(a.pos);
        _wn.copy(f.target).sub(_wp).multiplyScalar(f.magnitude * 0.5);
        a.vel.addScaledVector(_wn, a.invMass * sdt);
        a.angVel.addScaledVector(_c1.crossVectors(_ra, _wn), a.invI * sdt);
      }

      a.vel.multiplyScalar(0.96);
      a.angVel.multiplyScalar(0.94);
      a.pos.addScaledVector(a.vel, sdt);
      applyRotDelta(a.quat, _c1.copy(a.angVel).multiplyScalar(sdt));
      this.updateBox(a);

      if (a.pos.distanceToSquared(this.broadphasePos) > this.broadphaseMargin * this.broadphaseMargin * 0.25) {
        const tb = performance.now();
        this.updateBroadphase();
        this.prof.broadMs += performance.now() - tb;
        this.prof.broadCount++;
      }

      const to = performance.now();
      for (let it = 0; it < iters; it++) {
        for (const obs of this.potentialContacts) {
          if (a.box.intersectsBox(obs.box)) {
            this.prof.narrowPairs++;
            this.resolveObstacle(a, obs);
          }
        }
        this.updateBox(a);
      }
      this.prof.obsMs += performance.now() - to;

      // Clamp total displacement to MAX_NUDGE_CM (5 cm)
      _dp.copy(a.pos).sub(a.startPos);
      const dist = _dp.length();
      if (dist > MAX_NUDGE_CM) {
        a.pos.copy(a.startPos).addScaledVector(_dp, MAX_NUDGE_CM / dist);
      }

      const invDt = 1 / sdt;
      a.vel.copy(a.pos).sub(a.prevPos).multiplyScalar(invDt);
      _dq.copy(a.quat).multiply(a.prevQuat.clone().invert());
      if (_dq.w < 0) _dq.set(-_dq.x, -_dq.y, -_dq.z, -_dq.w);
      const angle = 2 * Math.acos(Math.min(1, Math.max(-1, _dq.w)));
      const sinHalf = Math.sqrt(Math.max(0, 1 - _dq.w * _dq.w));
      if (sinHalf > 1e-6) {
        a.angVel.set(_dq.x / sinHalf, _dq.y / sinHalf, _dq.z / sinHalf).multiplyScalar(angle * invDt);
      } else {
        a.angVel.set(0, 0, 0);
      }
    }

    a.item.mesh.position.copy(a.pos);
    a.item.mesh.quaternion.copy(a.quat);
    this.prof.stepMs += performance.now() - tStep;
  }

  profSummary(): string {
    const p = this.prof;
    const n = Math.max(1, p.steps);
    const other = Math.max(0, p.stepMs - p.broadMs - p.obsMs);
    return (
      `prof (${p.steps} steps, ${this.potentialContacts.length}/${this.obstacles.length} broad): ` +
      `init=${p.initMs.toFixed(1)}ms, avgStep=${(p.stepMs / n).toFixed(2)}ms ` +
      `[obs=${(p.obsMs / n).toFixed(2)}ms (${(p.narrowPairs / n).toFixed(1)} pairs), ` +
      `broad=${p.broadMs.toFixed(2)}ms(${p.broadCount}x), other=${(other / n).toFixed(2)}ms]`
    );
  }
}
