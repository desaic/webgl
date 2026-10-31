// three.js board: squares, pieces, highlights, picking and a short move animation.
import * as THREE from "three";
import { colorOf, fileOf, rankOf, typeOf, WHITE } from "./rules.ts";
import { makePiece } from "./Pieces.ts";

const LIGHT = new THREE.Color(0xe8d9b5);
const DARK = new THREE.Color(0xa67c52);
const SELECTED = new THREE.Color(0xf2d03b);
const LAST_MOVE = new THREE.Color(0xb7c85a);
const CHECK = new THREE.Color(0xd9402f);

export interface Highlights {
  selected: number; // -1 for none
  targets: number[];
  captures: number[]; // subset of targets that capture (drawn as rings)
  last: number[]; // from, to of the previous move
  check: number; // king square in check or -1
}

export class Board3D {
  private renderer: THREE.WebGLRenderer;
  private scene = new THREE.Scene();
  private camera = new THREE.PerspectiveCamera(38, 1, 0.1, 100);
  private squares: THREE.Mesh[] = [];
  private markers: THREE.Mesh[] = [];
  private pieces = new Map<number, THREE.Object3D>();
  private pieceRoot = new THREE.Group();
  private ray = new THREE.Raycaster();
  private flipped = false;
  private dirty = true;
  private anim: { obj: THREE.Object3D; from: THREE.Vector3; to: THREE.Vector3; t0: number; done: () => void } | null = null;
  private hl: Highlights = { selected: -1, targets: [], captures: [], last: [], check: -1 };

  constructor(private container: HTMLElement, private onSquare: (sq: number) => void) {
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    container.appendChild(this.renderer.domElement);
    this.scene.background = new THREE.Color(0x2b2724);

    this.scene.add(new THREE.HemisphereLight(0xffffff, 0x6a5a48, 1.1));
    const sun = new THREE.DirectionalLight(0xffffff, 1.6);
    sun.position.set(-4, 10, 6);
    sun.castShadow = true;
    sun.shadow.mapSize.set(2048, 2048);
    Object.assign(sun.shadow.camera, { left: -6, right: 6, top: 6, bottom: -6, near: 1, far: 30 });
    this.scene.add(sun);

    this.buildBoard();
    this.scene.add(this.pieceRoot);
    this.setFlipped(false);

    const el = this.renderer.domElement;
    let downAt: [number, number] | null = null;
    el.addEventListener("pointerdown", (e) => (downAt = [e.clientX, e.clientY]));
    el.addEventListener("pointerup", (e) => {
      if (!downAt) return;
      const moved = Math.hypot(e.clientX - downAt[0], e.clientY - downAt[1]);
      downAt = null;
      if (moved > 10) return;
      const s = this.pick(e.clientX, e.clientY);
      if (s >= 0) this.onSquare(s);
    });
    new ResizeObserver(() => this.resize()).observe(container);
    this.resize();
    requestAnimationFrame(this.frame);
  }

  private squarePos(s: number) {
    return new THREE.Vector3(fileOf(s) - 3.5, 0, 3.5 - rankOf(s));
  }

  private buildBoard() {
    const frame = new THREE.Mesh(
      new THREE.BoxGeometry(8.7, 0.3, 8.7),
      new THREE.MeshStandardMaterial({ color: 0x4a3426, roughness: 0.7 }),
    );
    frame.position.y = -0.16;
    frame.receiveShadow = true;
    this.scene.add(frame);

    const geo = new THREE.PlaneGeometry(1, 1);
    geo.rotateX(-Math.PI / 2);
    for (let s = 0; s < 64; s++) {
      const mat = new THREE.MeshStandardMaterial({ roughness: 0.8 });
      const m = new THREE.Mesh(geo, mat);
      m.position.copy(this.squarePos(s));
      m.receiveShadow = true;
      m.userData.sq = s;
      this.squares.push(m);
      this.scene.add(m);
    }
    this.paintSquares();
  }

  private paintSquares() {
    for (let s = 0; s < 64; s++) {
      const dark = (fileOf(s) + rankOf(s)) % 2 === 0;
      const c = (dark ? DARK : LIGHT).clone();
      const h = this.hl;
      if (h.last.includes(s)) c.lerp(LAST_MOVE, 0.55);
      if (s === h.selected) c.lerp(SELECTED, 0.75);
      if (s === h.check) c.lerp(CHECK, 0.8);
      (this.squares[s].material as THREE.MeshStandardMaterial).color.copy(c);
    }
  }

  setHighlights(h: Highlights) {
    this.hl = h;
    this.paintSquares();
    for (const m of this.markers) {
      this.scene.remove(m);
      m.geometry.dispose();
      (m.material as THREE.Material).dispose();
    }
    this.markers = [];
    for (const s of h.targets) {
      const capture = h.captures.includes(s);
      const geo = capture ? new THREE.RingGeometry(0.38, 0.47, 32) : new THREE.CircleGeometry(0.15, 24);
      geo.rotateX(-Math.PI / 2);
      const m = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ color: capture ? 0xc0392b : 0x2e6f40, transparent: true, opacity: 0.85 }));
      m.position.copy(this.squarePos(s));
      m.position.y = 0.012;
      this.markers.push(m);
      this.scene.add(m);
    }
    this.dirty = true;
  }

  // rebuild the piece meshes from a 64 entry board array
  setPosition(board: number[]) {
    this.pieceRoot.clear();
    this.pieces.clear();
    for (let s = 0; s < 64; s++) {
      const p = board[s];
      if (!p) continue;
      const obj = makePiece(typeOf(p), colorOf(p) === WHITE);
      obj.position.copy(this.squarePos(s));
      obj.userData.sq = s;
      this.pieceRoot.add(obj);
      this.pieces.set(s, obj);
    }
    this.dirty = true;
  }

  // slide the piece on `from` to `to`, then call done (which should call setPosition)
  animateMove(from: number, to: number, done: () => void) {
    const obj = this.pieces.get(from);
    if (!obj) return done();
    const target = this.squarePos(to);
    // pieces that are captured stay on the board until done() redraws it
    this.anim = { obj, from: obj.position.clone(), to: target, t0: performance.now(), done };
    this.dirty = true;
  }

  setFlipped(f: boolean) {
    this.flipped = f;
    this.resize();
    this.dirty = true;
  }

  private pick(clientX: number, clientY: number): number {
    const r = this.renderer.domElement.getBoundingClientRect();
    const ndc = new THREE.Vector2(((clientX - r.left) / r.width) * 2 - 1, -((clientY - r.top) / r.height) * 2 + 1);
    this.ray.setFromCamera(ndc, this.camera);
    const hits = this.ray.intersectObjects([...this.pieceRoot.children, ...this.squares], true);
    for (const h of hits) {
      let o: THREE.Object3D | null = h.object;
      while (o) {
        if (o.userData.sq !== undefined) return o.userData.sq;
        o = o.parent;
      }
    }
    return -1;
  }

  private resize() {
    const w = this.container.clientWidth, h = Math.max(1, this.container.clientHeight);
    this.renderer.setSize(w, h);
    this.camera.aspect = w / h;
    // pull the camera back on narrow screens so the whole board stays visible
    const dist = Math.max(12.5, 16.5 / this.camera.aspect);
    const dir = new THREE.Vector3(0, 0.82, 0.58).normalize().multiplyScalar(dist);
    if (this.flipped) dir.z = -dir.z;
    this.camera.position.copy(dir);
    this.camera.lookAt(0, 0, 0);
    this.camera.updateProjectionMatrix();
    this.dirty = true;
  }

  private frame = (now: number) => {
    requestAnimationFrame(this.frame);
    if (this.anim) {
      const a = this.anim;
      const t = Math.min(1, (now - a.t0) / 180);
      const e = t * t * (3 - 2 * t);
      a.obj.position.lerpVectors(a.from, a.to, e);
      a.obj.position.y = Math.sin(t * Math.PI) * 0.25;
      this.dirty = true;
      if (t >= 1) {
        this.anim = null;
        a.done();
      }
    }
    if (!this.dirty) return;
    this.dirty = false;
    this.renderer.render(this.scene, this.camera);
  };
}
