import * as THREE from "three";
import { MeshBVH } from "three-mesh-bvh";

// Report-only collision check of one fruit against the container and the other fruits.
// Surface samples of each mesh are tested against the other mesh with a BVH closest point query.
// A sample is inside when the vector from its closest surface point points against that face's
// normal. Container normals point out of the cavity, so a sample outside the container mesh is the
// one that penetrates the wall. Depth is a sampled lower bound on the true penetration.

export interface Contact {
  label: string;
  depth: number; // deepest sample inside the other mesh, 0 if none
  gap: number; // smallest distance of a non-penetrating sample, Infinity if none within range
}

const SEARCH = 4; // cm, closest point search radius
const TOUCH = 0.3; // cm, gap below which two meshes count as touching
const N_SAMPLES = 2000;

const bvhs = new WeakMap<THREE.BufferGeometry, MeshBVH>();
function bvhOf(g: THREE.BufferGeometry): MeshBVH {
  let b = bvhs.get(g);
  if (!b) {
    b = new MeshBVH(g);
    bvhs.set(g, b);
  }
  return b;
}

const sampleCache = new WeakMap<THREE.BufferGeometry, THREE.Vector3[]>();
// points spread evenly over the surface (triangle area weighted, fixed seed)
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
  let seed = 2463534242;
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

const _p = new THREE.Vector3();
const _inv = new THREE.Matrix4();
const _hit = { point: new THREE.Vector3(), distance: 0, faceIndex: 0 };

const _ray = new THREE.Ray();
const RAY_DIRS = [new THREE.Vector3(1, 0.1234, 0.0567).normalize(), new THREE.Vector3(0.0789, 1, 0.1357).normalize(), new THREE.Vector3(0.0246, 0.0913, 1).normalize()];
function insideByRays(bvh: MeshBVH, p: THREE.Vector3): boolean {
  let votes = 0;
  for (const d of RAY_DIRS) {
    _ray.set(p, d);
    if (bvh.raycast(_ray, THREE.DoubleSide).length % 2 === 1) votes++;
  }
  return votes >= 2;
}

// samples of `from` against the surface of `to`. flip: the free side is inside the mesh volume (container).
function probe(from: THREE.Mesh, to: THREE.Mesh, flip: boolean, acc: Contact) {
  to.updateMatrixWorld(true);
  from.updateMatrixWorld(true);
  _inv.copy(to.matrixWorld).invert();
  const bvh = bvhOf(to.geometry);
  const scale = to.scale.x;
  for (const lp of samplesOf(from.geometry)) {
    _p.copy(lp).applyMatrix4(from.matrixWorld).applyMatrix4(_inv);
    const hit = bvh.closestPointToPoint(_p, _hit, 0, SEARCH / scale);
    if (!hit) continue;
    // inside the mesh volume: majority of three oblique ray parities. thin shells such as leaves and open
    // meshes break the sign of the closest face normal, but parity along a few rays survives them.
    // for the container the free side is the volume itself, so a sample outside it is the penetrating one.
    const inMesh = insideByRays(bvh, _p);
    const inside = flip ? !inMesh : inMesh;
    const d = hit.distance * scale;
    if (inside) acc.depth = Math.max(acc.depth, d);
    else acc.gap = Math.min(acc.gap, d);
  }
}

export function collisions(self: THREE.Mesh, fruits: { label: string; mesh: THREE.Mesh }[], containers: THREE.Mesh[]): Contact[] {
  const box = new THREE.Box3().setFromObject(self).expandByScalar(TOUCH + 1);
  const out: Contact[] = [];
  const b = new THREE.Box3();
  for (const f of fruits) {
    if (f.mesh === self) continue;
    f.mesh.updateMatrixWorld(true);
    if (!b.setFromObject(f.mesh).intersectsBox(box)) continue;
    const c: Contact = { label: f.label, depth: 0, gap: Infinity };
    probe(self, f.mesh, false, c);
    probe(f.mesh, self, false, c);
    if (c.depth > 0 || c.gap < TOUCH) out.push(c);
  }
  containers.forEach((m, i) => {
    const c: Contact = { label: containers.length > 1 ? "container " + (i + 1) : "container", depth: 0, gap: Infinity };
    probe(self, m, true, c);
    if (c.depth > 0 || c.gap < TOUCH) out.push(c);
  });
  return out.sort((a, b) => b.depth - a.depth || a.gap - b.gap);
}
