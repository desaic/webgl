// Chess pieces: simplified meshes from ignore/pieces.blend (see ../../tools), one solid colour per side.
// The OBJ files only contain positions and triangles; faces are flat shaded, so no normals are needed.
import * as THREE from "three";
import { P, N, B, R, Q, K } from "./rules.ts";

const FILES: Record<number, string> = { [P]: "pawn", [R]: "rook", [N]: "knight", [B]: "bishop", [Q]: "queen", [K]: "king" };

export const WHITE_COLOR = 0xf1e9d8;
export const BLACK_COLOR = 0x3b2a22;

const geometries = new Map<number, THREE.BufferGeometry>();

// minimal OBJ reader: "v x y z" and "f a b c" lines only
function parseObj(text: string): THREE.BufferGeometry {
  const pos: number[] = [];
  const idx: number[] = [];
  for (const line of text.split("\n")) {
    if (line.startsWith("v ")) {
      const p = line.split(" ");
      pos.push(+p[1], +p[2], +p[3]);
    } else if (line.startsWith("f ")) {
      const p = line.split(" ");
      idx.push(+p[1] - 1, +p[2] - 1, +p[3] - 1);
    }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute("position", new THREE.Float32BufferAttribute(pos, 3));
  g.setIndex(idx);
  return g;
}

// fetch all piece meshes; call (and await) before building the board
export async function loadPieces(): Promise<void> {
  await Promise.all(
    Object.entries(FILES).map(async ([type, name]) => {
      const res = await fetch(`pieces/${name}.obj`);
      if (!res.ok) throw new Error(`cannot load pieces/${name}.obj: ${res.status}`);
      geometries.set(+type, parseObj(await res.text()));
    }),
  );
}

const materials = new Map<number, THREE.MeshStandardMaterial>();
function pieceMaterial(white: boolean) {
  const key = white ? 1 : 0;
  let m = materials.get(key);
  if (!m) {
    m = new THREE.MeshStandardMaterial({ color: white ? WHITE_COLOR : BLACK_COLOR, roughness: 0.45, metalness: 0.05, flatShading: true });
    materials.set(key, m);
  }
  return m;
}

export function makePiece(type: number, white: boolean): THREE.Group {
  const mesh = new THREE.Mesh(geometries.get(type)!, pieceMaterial(white));
  mesh.castShadow = true;
  // knights are modelled facing +x: white ones face right, black ones left
  if (type === N && !white) mesh.rotation.y = Math.PI;
  const outer = new THREE.Group();
  outer.add(mesh);
  return outer;
}
