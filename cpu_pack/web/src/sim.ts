import * as THREE from "three";
import { MeshBVH } from "three-mesh-bvh";
import { Item } from "./scene";

const N_SAMPLES = 80;
const MAX_NUDGE_CM = 5;
const bvhs = new WeakMap<THREE.BufferGeometry, MeshBVH>();
const sampleCache = new WeakMap<THREE.BufferGeometry, THREE.Vector3[]>();

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
  const idx = g.index;
  const nt = (idx ? idx.count : pos.count) / 3;
  const vi = (t: number, k: number) => (idx ? idx.getX(t * 3 + k) : t * 3 + k);
  const a = new THREE.Vector3();
  const b = new THREE.Vector3();
  const c = new THREE.Vector3();
  const cum = new Float64Array(nt);
  let total = 0;
  for (let t = 0; t < nt; t++) {
    a.fromBufferAttribute(pos, vi(t, 0));
    b.fromBufferAttribute(pos, vi(t, 1)).sub(a);
    c.fromBufferAttribute(pos, vi(t, 2)).sub(a);
    total += b.cross(c).length() * 0.5;
    cum[t] = total;
  }
  let seed = 13374242;
  const rnd = () => ((seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0) / 4294967296);
  s = [];
  for (let i = 0; i < N_SAMPLES; i++) {
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
  dir: THREE.Vector3; // world unit vector
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
  containers: THREE.Mesh[] = [];
  force: ForceSpec | null = null;
  running = false;
  stepsLeft = 0;

  private broadphasePos = new THREE.Vector3();
  private broadphaseMargin = 0;

  init(selected: Item | null, items: Item[], containers: THREE.Mesh[]) {
    this.containers = containers;
    this.obstacles = [];
    this.potentialContacts = [];
    this.active = null;
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
    const bvhObs = bvhOf(obs.item.full);
    const bvhA = bvhOf(a.item.full);
    _mat.compose(a.pos, a.quat, _c2.set(a.scale, a.scale, a.scale));
    _inv.copy(_mat).invert();

    let bestDepth = 0;
    const bestWp = new THREE.Vector3();
    const bestWn = new THREE.Vector3();

    // 1. active samples inside static obstacle
    for (const lp of samplesOf(a.item.full)) {
      _wp.copy(lp).applyMatrix4(_mat);
      _p.copy(_wp).applyMatrix4(obs.inv);
      const hit = bvhObs.closestPointToPoint(_p, _hit, 0, (a.radius + obs.radius) / obs.scale);
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
    for (const lp of samplesOf(obs.item.full)) {
      _wp.copy(lp).applyMatrix4(obs.mat);
      _p.copy(_wp).applyMatrix4(_inv);
      const hit = bvhA.closestPointToPoint(_p, _hit, 0, (a.radius + obs.radius) / a.scale);
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

  private resolveContainer(a: Body, cont: THREE.Mesh) {
    cont.updateMatrixWorld(true);
    const sc = cont.scale.x;
    _inv.copy(cont.matrixWorld).invert();
    const bvh = bvhOf(cont.geometry);
    _mat.compose(a.pos, a.quat, _c2.set(a.scale, a.scale, a.scale));

    let bestDepth = 0;
    const bestWp = new THREE.Vector3();
    const bestWn = new THREE.Vector3();

    for (const lp of samplesOf(a.item.full)) {
      _wp.copy(lp).applyMatrix4(_mat);
      _p.copy(_wp).applyMatrix4(_inv);
      const hit = bvh.closestPointToPoint(_p, _hit, 0, (a.radius * 2) / sc);
      if (!hit || insideByRays(bvh, _p)) continue;
      const d = hit.distance * sc;
      if (d > bestDepth) {
        bestDepth = d;
        bestWp.copy(_wp);
        bestWn.copy(hit.point).sub(_p).transformDirection(cont.matrixWorld).normalize();
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

  step(dt = 1 / 60, substeps = 4) {
    const a = this.active;
    if (!a) return;
    const sdt = dt / substeps;

    for (let s = 0; s < substeps; s++) {
      a.prevPos.copy(a.pos);
      a.prevQuat.copy(a.quat);

      if (this.force && this.force.item === a.item) {
        _wp.copy(this.force.localPoint).multiplyScalar(a.scale).applyQuaternion(a.quat).add(a.pos);
        _ra.copy(_wp).sub(a.pos);
        _wn.copy(this.force.dir).multiplyScalar(this.force.magnitude);
        a.vel.addScaledVector(_wn, a.invMass * sdt);
        a.angVel.addScaledVector(_c1.crossVectors(_ra, _wn), a.invI * sdt);
      }

      a.vel.multiplyScalar(0.96);
      a.angVel.multiplyScalar(0.94);
      a.pos.addScaledVector(a.vel, sdt);
      applyRotDelta(a.quat, _c1.copy(a.angVel).multiplyScalar(sdt));
      this.updateBox(a);

      if (a.pos.distanceToSquared(this.broadphasePos) > this.broadphaseMargin * this.broadphaseMargin * 0.25) {
        this.updateBroadphase();
      }

      for (const cont of this.containers) this.resolveContainer(a, cont);
      for (const obs of this.potentialContacts) {
        if (a.box.intersectsBox(obs.box)) this.resolveObstacle(a, obs);
      }

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
  }
}
