import * as THREE from "three";

// Cell based culling, like a city streamer: items are binned into cubic cells by their center.
// A cell is drawn only when it is inside the view frustum and not hidden behind occupied cells.
// Occlusion is a low resolution ray buffer: every occupied cell (shrunk to its core) is treated as a
// solid cube, and a cell is hidden when, on every pixel that sees it, a solid cube ends before it begins.

const W = 112;
const H = 64;

export interface CullItem {
  pos: THREE.Vector3;
  radius: number; // world radius of the mesh bounding sphere
  keep: boolean; // never cull (selected, user pinned)
}

interface Cell {
  items: number[];
  lo: THREE.Vector3; // cell box expanded by the largest item radius
  hi: THREE.Vector3;
  clo: THREE.Vector3; // solid core
  chi: THREE.Vector3;
}

export class Culler {
  cellSize = 20;
  coreFrac = 0.6; // fraction of the cell edge assumed solid
  minItems = 3; // a cell with fewer items does not block
  occlusion = true;
  stats = { cells: 0, drawn: 0, items: 0, total: 0 };

  private occFar = new Float32Array(W * H);
  private rays = new Float32Array(W * H * 3);
  private frustum = new THREE.Frustum();
  private pv = new THREE.Matrix4();
  private tmpBox = new THREE.Box3();
  private c = new THREE.Vector3();

  // returns, per item, whether it should be drawn
  run(items: CullItem[], camera: THREE.PerspectiveCamera, occlusion: boolean): boolean[] {
    const cs = this.cellSize;
    const cells = new Map<string, Cell>();
    let maxR = 0;
    for (const it of items) maxR = Math.max(maxR, it.radius);
    items.forEach((it, i) => {
      const x = Math.floor(it.pos.x / cs), y = Math.floor(it.pos.y / cs), z = Math.floor(it.pos.z / cs);
      const key = x + "," + y + "," + z;
      let c = cells.get(key);
      if (!c) {
        const m = (1 - this.coreFrac) * 0.5 * cs;
        c = {
          items: [],
          lo: new THREE.Vector3(x * cs - maxR, y * cs - maxR, z * cs - maxR),
          hi: new THREE.Vector3((x + 1) * cs + maxR, (y + 1) * cs + maxR, (z + 1) * cs + maxR),
          clo: new THREE.Vector3(x * cs + m, y * cs + m, z * cs + m),
          chi: new THREE.Vector3((x + 1) * cs - m, (y + 1) * cs - m, (z + 1) * cs - m),
        };
        cells.set(key, c);
      }
      c.items.push(i);
    });

    camera.updateMatrixWorld();
    this.pv.multiplyMatrices(camera.projectionMatrix, camera.matrixWorldInverse);
    this.frustum.setFromProjectionMatrix(this.pv);
    const inView: Cell[] = [];
    for (const c of cells.values()) {
      this.tmpBox.min.copy(c.lo);
      this.tmpBox.max.copy(c.hi);
      if (this.frustum.intersectsBox(this.tmpBox)) inView.push(c);
    }

    const draw: boolean[] = new Array(items.length).fill(false);
    let drawnCells = 0;
    const hidden = new Set<Cell>();
    if (occlusion) this.occlude(inView, camera, hidden);
    for (const c of inView) {
      if (hidden.has(c)) continue;
      drawnCells++;
      for (const i of c.items) draw[i] = true;
    }
    items.forEach((it, i) => {
      if (it.keep) draw[i] = true;
    });
    this.stats = { cells: inView.length, drawn: drawnCells, items: draw.filter(Boolean).length, total: items.length };
    return draw;
  }

  private buildRays(camera: THREE.PerspectiveCamera) {
    const inv = camera.projectionMatrixInverse;
    const mw = camera.matrixWorld;
    const o = new THREE.Vector3().setFromMatrixPosition(mw);
    const p = new THREE.Vector3();
    for (let j = 0; j < H; j++) {
      for (let i = 0; i < W; i++) {
        p.set(((i + 0.5) / W) * 2 - 1, 1 - ((j + 0.5) / H) * 2, 1).applyMatrix4(inv).applyMatrix4(mw).sub(o).normalize();
        const k = (j * W + i) * 3;
        this.rays[k] = p.x;
        this.rays[k + 1] = p.y;
        this.rays[k + 2] = p.z;
      }
    }
    return o;
  }

  // screen rectangle (pixel indices, inclusive) of a box; whole screen when a corner is behind the camera
  private rect(lo: THREE.Vector3, hi: THREE.Vector3): [number, number, number, number] {
    let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    const v = this.c;
    for (let m = 0; m < 8; m++) {
      v.set(m & 1 ? hi.x : lo.x, m & 2 ? hi.y : lo.y, m & 4 ? hi.z : lo.z);
      const w = this.pv.elements[3] * v.x + this.pv.elements[7] * v.y + this.pv.elements[11] * v.z + this.pv.elements[15];
      if (w <= 1e-4) return [0, 0, W - 1, H - 1];
      v.applyMatrix4(this.pv);
      x0 = Math.min(x0, v.x); x1 = Math.max(x1, v.x);
      y0 = Math.min(y0, v.y); y1 = Math.max(y1, v.y);
    }
    const px0 = Math.max(0, Math.floor(((x0 + 1) * 0.5) * W - 0.5));
    const px1 = Math.min(W - 1, Math.ceil(((x1 + 1) * 0.5) * W - 0.5));
    const py0 = Math.max(0, Math.floor(((1 - y1) * 0.5) * H - 0.5));
    const py1 = Math.min(H - 1, Math.ceil(((1 - y0) * 0.5) * H - 0.5));
    return [px0, py0, px1, py1];
  }

  // ray vs box slab test, writes near and far distances to out (far <= near when missed)
  private slab(o: THREE.Vector3, k: number, lo: THREE.Vector3, hi: THREE.Vector3, out: number[]) {
    let t0 = 0, t1 = Infinity;
    for (let a = 0; a < 3; a++) {
      const d = this.rays[k + a];
      const oa = a === 0 ? o.x : a === 1 ? o.y : o.z;
      const la = a === 0 ? lo.x : a === 1 ? lo.y : lo.z;
      const ha = a === 0 ? hi.x : a === 1 ? hi.y : hi.z;
      if (Math.abs(d) < 1e-9) {
        if (oa < la || oa > ha) {
          out[0] = 1; out[1] = 0;
          return;
        }
        continue;
      }
      let a0 = (la - oa) / d, a1 = (ha - oa) / d;
      if (a0 > a1) { const t = a0; a0 = a1; a1 = t; }
      if (a0 > t0) t0 = a0;
      if (a1 < t1) t1 = a1;
    }
    out[0] = t0;
    out[1] = t1;
  }

  private occlude(inView: Cell[], camera: THREE.PerspectiveCamera, hidden: Set<Cell>) {
    const o = this.buildRays(camera);
    this.occFar.fill(Infinity);
    const r = [0, 0];
    const inside = (p: THREE.Vector3, lo: THREE.Vector3, hi: THREE.Vector3) =>
      p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y && p.z >= lo.z && p.z <= hi.z;
    for (const c of inView) {
      if (c.items.length < this.minItems || inside(o, c.clo, c.chi)) continue;
      const [x0, y0, x1, y1] = this.rect(c.clo, c.chi);
      for (let j = y0; j <= y1; j++) {
        for (let i = x0; i <= x1; i++) {
          const p = j * W + i;
          this.slab(o, p * 3, c.clo, c.chi, r);
          if (r[1] > r[0] && r[0] > 0 && r[1] < this.occFar[p]) this.occFar[p] = r[1];
        }
      }
    }
    for (const c of inView) {
      if (inside(o, c.lo, c.hi)) continue;
      const [x0, y0, x1, y1] = this.rect(c.lo, c.hi);
      let seen = false, any = false;
      for (let j = y0; j <= y1 && !seen; j++) {
        for (let i = x0; i <= x1; i++) {
          const p = j * W + i;
          this.slab(o, p * 3, c.lo, c.hi, r);
          if (r[1] <= r[0]) continue;
          any = true;
          if (!(this.occFar[p] < r[0])) {
            seen = true;
            break;
          }
        }
      }
      if (any && !seen) hidden.add(c);
    }
  }
}

// vertex clustering decimation: snap vertices to a grid of about n cells across the longest side, merge the
// vertices of a cell into their average, and drop triangles that collapse. fast and good enough for far fruit.
export function decimate(geo: THREE.BufferGeometry, n: number): THREE.BufferGeometry {
  const pos = geo.getAttribute("position");
  const idx = geo.getIndex();
  geo.computeBoundingBox();
  const bb = geo.boundingBox!;
  const size = bb.getSize(new THREE.Vector3());
  const g = Math.max(size.x, size.y, size.z) / n;
  const cellOf = new Map<number, number>();
  const sum: number[] = [];
  const cnt: number[] = [];
  const remap = new Int32Array(pos.count);
  const nx = Math.ceil(size.x / g) + 1, ny = Math.ceil(size.y / g) + 1;
  for (let i = 0; i < pos.count; i++) {
    const x = pos.getX(i), y = pos.getY(i), z = pos.getZ(i);
    const key = Math.floor((x - bb.min.x) / g) + nx * (Math.floor((y - bb.min.y) / g) + ny * Math.floor((z - bb.min.z) / g));
    let id = cellOf.get(key);
    if (id === undefined) {
      id = cnt.length;
      cellOf.set(key, id);
      cnt.push(0);
      sum.push(0, 0, 0);
    }
    cnt[id]++;
    sum[id * 3] += x;
    sum[id * 3 + 1] += y;
    sum[id * 3 + 2] += z;
    remap[i] = id;
  }
  const out: number[] = [];
  const tris = (idx ? idx.count : pos.count) / 3;
  for (let t = 0; t < tris; t++) {
    const a = remap[idx ? idx.getX(3 * t) : 3 * t];
    const b = remap[idx ? idx.getX(3 * t + 1) : 3 * t + 1];
    const c = remap[idx ? idx.getX(3 * t + 2) : 3 * t + 2];
    if (a !== b && b !== c && a !== c) out.push(a, b, c);
  }
  const p = new Float32Array(cnt.length * 3);
  for (let i = 0; i < cnt.length; i++) {
    p[i * 3] = sum[i * 3] / cnt[i];
    p[i * 3 + 1] = sum[i * 3 + 1] / cnt[i];
    p[i * 3 + 2] = sum[i * 3 + 2] / cnt[i];
  }
  const res = new THREE.BufferGeometry();
  res.setAttribute("position", new THREE.BufferAttribute(p, 3));
  res.setIndex(out);
  res.computeVertexNormals();
  res.computeBoundingSphere();
  return res;
}
