// Procedural chess piece meshes (lathe profiles, no model files). One square is 1 unit wide.
import * as THREE from "three";
import { P, N, B, R, Q, K } from "./rules.ts";

const v = (pts: number[][]) => pts.map(([r, h]) => new THREE.Vector2(r, h));

// radius, height pairs from the base up; revolved around the y axis
const PROFILES: Record<number, THREE.Vector2[]> = {
  [P]: v([[0, 0], [0.3, 0], [0.31, 0.06], [0.22, 0.12], [0.12, 0.32], [0.17, 0.38], [0.17, 0.42], [0.1, 0.46]]),
  [R]: v([[0, 0], [0.34, 0], [0.35, 0.08], [0.25, 0.16], [0.2, 0.5], [0.3, 0.56], [0.3, 0.72], [0.2, 0.72], [0.2, 0.66], [0, 0.66]]),
  [B]: v([[0, 0], [0.33, 0], [0.34, 0.07], [0.22, 0.15], [0.12, 0.45], [0.2, 0.5], [0.12, 0.55]]),
  [Q]: v([[0, 0], [0.35, 0], [0.36, 0.08], [0.24, 0.17], [0.13, 0.6], [0.26, 0.7], [0.18, 0.76], [0.1, 0.8]]),
  [K]: v([[0, 0], [0.35, 0], [0.36, 0.08], [0.24, 0.17], [0.14, 0.62], [0.26, 0.7], [0.2, 0.8], [0.1, 0.82]]),
  [N]: v([[0, 0], [0.33, 0], [0.34, 0.07], [0.25, 0.14], [0, 0.14]]),
};

const lathe = new Map<number, THREE.BufferGeometry>();
function latheGeometry(type: number) {
  let g = lathe.get(type);
  if (!g) {
    g = new THREE.LatheGeometry(PROFILES[type], 28);
    lathe.set(type, g);
  }
  return g;
}

function ball(r: number, y: number, mat: THREE.Material) {
  const m = new THREE.Mesh(new THREE.SphereGeometry(r, 20, 14), mat);
  m.position.y = y;
  return m;
}

function box(w: number, h: number, d: number, y: number, mat: THREE.Material) {
  const m = new THREE.Mesh(new THREE.BoxGeometry(w, h, d), mat);
  m.position.y = y;
  return m;
}

// profile of a horse head in the x-y plane, facing +x so it reads from the side
function knightGeometry() {
  const s = new THREE.Shape();
  s.moveTo(-0.22, 0.14);
  s.lineTo(0.2, 0.14);
  s.lineTo(0.2, 0.34);
  s.lineTo(0.3, 0.46);
  s.lineTo(0.42, 0.5);
  s.lineTo(0.42, 0.58);
  s.lineTo(0.18, 0.8);
  s.lineTo(0.0, 0.84);
  s.lineTo(-0.1, 0.78);
  s.lineTo(-0.2, 0.7);
  s.lineTo(-0.28, 0.4);
  s.closePath();
  const g = new THREE.ExtrudeGeometry(s, { depth: 0.26, bevelEnabled: true, bevelSize: 0.03, bevelThickness: 0.03, bevelSegments: 2 });
  g.translate(0, 0, -0.13);
  return g;
}
let knightGeo: THREE.BufferGeometry | null = null;

export const WHITE_COLOR = 0xf1e9d8;
export const BLACK_COLOR = 0x3b2a22;

const materials = new Map<number, THREE.MeshStandardMaterial>();
export function pieceMaterial(white: boolean) {
  const key = white ? 1 : 0;
  let m = materials.get(key);
  if (!m) {
    m = new THREE.MeshStandardMaterial({
      color: white ? WHITE_COLOR : BLACK_COLOR,
      roughness: 0.45,
      metalness: 0.05,
    });
    materials.set(key, m);
  }
  return m;
}

export function makePiece(type: number, white: boolean): THREE.Group {
  const g = new THREE.Group();
  const mat = pieceMaterial(white);
  g.add(new THREE.Mesh(latheGeometry(type), mat));
  switch (type) {
    case P:
      g.add(ball(0.14, 0.56, mat));
      break;
    case B:
      g.add(ball(0.15, 0.68, mat));
      g.add(ball(0.04, 0.86, mat));
      break;
    case Q:
      g.add(ball(0.06, 0.88, mat));
      break;
    case K:
      g.add(box(0.07, 0.3, 0.07, 1.0, mat));
      g.add(box(0.22, 0.07, 0.07, 1.04, mat));
      break;
    case N:
      knightGeo = knightGeo || knightGeometry();
      g.add(new THREE.Mesh(knightGeo, mat));
      // white knights face right, black ones left
      g.rotation.y = white ? 0 : Math.PI;
      break;
  }
  g.traverse((o) => ((o as THREE.Mesh).isMesh && ((o as THREE.Mesh).castShadow = true)));
  const outer = new THREE.Group();
  outer.add(g);
  return outer;
}
