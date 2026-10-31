// Simplifies the raw piece meshes with meshoptimizer and writes board-ready OBJs.
//
//   cd web && npm run pieces      (after tools/extract_pieces.py has filled tools/raw/)
//
// Input:  tools/raw/<name>.obj   (v/f only, y up, base at y = 0, blend units)
// Output: web/public/pieces/<name>.obj  (v/f only, scaled so one board square is 1 unit)
import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { MeshoptSimplifier } from "../web/node_modules/meshoptimizer/meshopt_simplifier.js";

const here = dirname(fileURLToPath(import.meta.url));
const RAW = join(here, "raw");
const OUT = join(here, "..", "web", "public", "pieces");

// blend units -> board squares. A king (95 units tall) ends up about 1.5 squares high.
const SCALE = 0.016;

// triangles to keep (pieces are only ~60 px tall on screen); the knight has the most detail
const TARGET_TRIS = { pawn: 1200, rook: 1800, knight: 3600, bishop: 1800, queen: 2400, king: 2400 };

await MeshoptSimplifier.ready;

for (const [name, tris] of Object.entries(TARGET_TRIS)) {
  const text = readFileSync(join(RAW, name + ".obj"), "utf8");
  const pos = [];
  const idx = [];
  for (const line of text.split("\n")) {
    if (line.startsWith("v ")) {
      const p = line.split(" ");
      pos.push(+p[1], +p[2], +p[3]);
    } else if (line.startsWith("f ")) {
      const p = line.split(" ");
      idx.push(+p[1] - 1, +p[2] - 1, +p[3] - 1);
    }
  }
  const positions = new Float32Array(pos);
  let indices = new Uint32Array(idx);
  const before = indices.length / 3;

  // weld vertices that share a position so the simplifier sees one connected surface
  const remap = MeshoptSimplifier.generatePositionRemap(positions, 3);
  indices = indices.map((i) => remap[i]);

  const [simplified, error] = MeshoptSimplifier.simplify(indices, positions, 3, tris * 3, 0.1, ["Prune"]);
  // compactMesh renumbers `simplified` in place (vertices numbered by first use), so keep the old
  // numbers to look the positions up
  const old = simplified.slice();
  const [, unique] = MeshoptSimplifier.compactMesh(simplified);
  const compact = simplified;
  const order = new Int32Array(unique);
  for (let i = 0; i < compact.length; i++) order[compact[i]] = old[i];
  const lines = [`# ${name}: ${unique} vertices, ${compact.length / 3} triangles (from ${before}), error ${error.toFixed(4)}`];
  for (let v = 0; v < unique; v++) {
    const s = order[v] * 3;
    lines.push(`v ${(positions[s] * SCALE).toFixed(3)} ${(positions[s + 1] * SCALE).toFixed(3)} ${(positions[s + 2] * SCALE).toFixed(3)}`);
  }
  for (let t = 0; t < compact.length; t += 3) {
    lines.push(`f ${compact[t] + 1} ${compact[t + 1] + 1} ${compact[t + 2] + 1}`);
  }
  mkdirSync(OUT, { recursive: true });
  writeFileSync(join(OUT, name + ".obj"), lines.join("\n") + "\n");
  console.log(`${name.padEnd(7)} ${before} -> ${compact.length / 3} triangles, ${unique} vertices, relative error ${error.toFixed(4)}`);
}
