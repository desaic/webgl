import * as THREE from "three";

// Cell based culling: items are binned into cubic world cells of size `cellSize`.
// When active, only the cell closest in depth to the camera near the view direction and its 26 neighbors are drawn.

export interface CullItem {
  pos: THREE.Vector3;
  radius: number; // world radius of the mesh bounding sphere
  keep: boolean; // never cull (selected, user pinned)
}

interface Cell {
  gx: number;
  gy: number;
  gz: number;
  cx: number;
  cy: number;
  cz: number;
  items: number[];
}

export class Culler {
  cellSize = 20;
  latWeight = 3; // penalty per unit of perpendicular distance from the view ray
  stats = { cells: 0, drawn: 0, items: 0, total: 0 };
  drawnCells: { gx: number; gy: number; gz: number; center: boolean }[] = [];
  debug = "";

  // returns, per item, whether it should be drawn
  run(items: CullItem[], camera: THREE.PerspectiveCamera, occlusion: boolean): boolean[] {
    this.drawnCells = [];
    this.debug = "";
    if (!occlusion) {
      const draw = new Array(items.length).fill(true);
      this.stats = { cells: 0, drawn: 0, items: items.length, total: items.length };
      return draw;
    }

    const cs = this.cellSize;
    const cells = new Map<string, Cell>();
    items.forEach((it, i) => {
      const gx = Math.floor(it.pos.x / cs), gy = Math.floor(it.pos.y / cs), gz = Math.floor(it.pos.z / cs);
      const key = gx + "," + gy + "," + gz;
      let c = cells.get(key);
      if (!c) {
        c = { gx, gy, gz, cx: (gx + 0.5) * cs, cy: (gy + 0.5) * cs, cz: (gz + 0.5) * cs, items: [] };
        cells.set(key, c);
      }
      c.items.push(i);
    });

    camera.updateMatrixWorld();
    const view = camera.matrixWorldInverse;
    const v = new THREE.Vector3();
    const pad = 0.866 * cs;

    let center: Cell | null = null;
    let bestScore = Infinity;
    let bestDepth = 0;
    let bestLat = 0;

    for (const c of cells.values()) {
      v.set(c.cx, c.cy, c.cz).applyMatrix4(view);
      const depth = -v.z;
      if (depth <= -pad) continue;
      const lat = Math.hypot(v.x, v.y);
      const score = depth + this.latWeight * lat;
      if (score < bestScore) {
        bestScore = score;
        bestDepth = depth;
        bestLat = lat;
        center = c;
      }
    }
    const draw: boolean[] = new Array(items.length).fill(false);
    let drawnCells = 0;
    if (center) {
      for (let dx = -1; dx <= 1; dx++) {
        for (let dy = -1; dy <= 1; dy++) {
          for (let dz = -1; dz <= 1; dz++) {
            const n = cells.get((center.gx + dx) + "," + (center.gy + dy) + "," + (center.gz + dz));
            if (!n) continue;
            drawnCells++;
            this.drawnCells.push({ gx: n.gx, gy: n.gy, gz: n.gz, center: dx === 0 && dy === 0 && dz === 0 });
            for (const i of n.items) draw[i] = true;
          }
        }
      }
    }
    items.forEach((it, i) => {
      if (it.keep) draw[i] = true;
    });
    this.stats = { cells: cells.size, drawn: drawnCells, items: draw.filter(Boolean).length, total: items.length };
    const idx = this.drawnCells.map((c) => (c.center ? "*" : "") + c.gx + "," + c.gy + "," + c.gz).join(" ");
    this.debug = center
      ? `center (${center.gx},${center.gy},${center.gz}) z=${bestDepth.toFixed(1)} lat=${bestLat.toFixed(1)} score=${bestScore.toFixed(1)}\ncells (${drawnCells}/${cells.size}): ${idx}`
      : `no center cell (${cells.size} total)`;
    return draw;
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
