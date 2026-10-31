// three.js board: squares, pieces, highlights, picking and a short move animation.
import * as THREE from "three";
import { colorOf, fileOf, rankOf, typeOf, WHITE } from "./rules.ts";
import { makePiece } from "./Pieces.ts";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { DragControls } from "three/examples/jsm/controls/DragControls.js";
import { Line2 } from "three/examples/jsm/lines/Line2.js";
import { LineGeometry } from "three/examples/jsm/lines/LineGeometry.js";
import { LineMaterial } from "three/examples/jsm/lines/LineMaterial.js";

const LIGHT = new THREE.Color(0xe8d9b5);
const DARK = new THREE.Color(0xa67c52);
const SELECTED = new THREE.Color(0xf2d03b);
const LAST_MOVE = new THREE.Color(0xb7c85a);
const CHECK = new THREE.Color(0xd9402f);

const PAN_LIMIT = 1.5; // how far the view can be panned from the board centre
const DROP_COLOR = 0x5fae6e; // outline of the square a held piece would land on (muted green)
const DROP_WIDTH = 7; // in screen pixels, whatever the zoom or tilt
const DROP_OPACITY = 0.7;
const LABEL_COLOR = "#d4d4d4"; // grayish white
const LABEL_FONT = '"Arial", "Liberation Sans", "Helvetica Neue", Helvetica, sans-serif';
const LABEL_SIZE = 0.3; // world units
// Half the width of the frame around the squares (the squares themselves are 4 each way).
const FRAME_HALF = 4.35;
const LABEL_EDGE = (4 + FRAME_HALF) / 2; // centre of the strip between squares and frame edge
// the view is centred a little toward +z (white's side) instead of on the board centre;
// mirrored when the board is flipped so the composition on screen stays the same
const VIEW_CENTER_Z = 0.6;
const LIFT = 0.35; // height of a piece while it is being dragged

export interface Highlights {
  selected: number; // -1 for none
  targets: number[];
  captures: number[]; // subset of targets that capture (drawn as rings)
  last: number[]; // from, to of the previous move
  check: number; // king square in check or -1
}

export interface Handlers {
  onSquare: (sq: number) => void; // plain click on a square or piece
  canDrag: (sq: number) => boolean; // may the piece on this square be picked up?
  onDragStart: (sq: number) => void;
  // return true if the drop is a legal move (the controller then plays it), false to snap back
  onDrop: (from: number, to: number) => boolean;
}

export class Board3D {
  private renderer: THREE.WebGLRenderer;
  private scene = new THREE.Scene();
  private camera = new THREE.PerspectiveCamera(38, 1, 0.1, 100);
  private squares: THREE.Mesh[] = [];
  private markers: THREE.Mesh[] = [];
  private pieces = new Map<number, THREE.Object3D>();
  private pieceRoot = new THREE.Object3D();
  private dragList: THREE.Object3D[] = [];
  private orbit!: OrbitControls;
  private drag!: DragControls;
  private dropBox!: Line2;
  // letters a..h on both the near and far edge, numbers 1..8 on both the right and left edge
  // (index i is the glyph, i + 8 its copy on the opposite edge)
  private fileLabels: THREE.Mesh[] = [];
  private rankLabels: THREE.Mesh[] = [];
  private dragSq = -1; // square of the piece being dragged, -1 if none
  private pointer = new THREE.Vector2();
  private ground = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
  private ray = new THREE.Raycaster();
  private flipped = false;
  private dirty = true;
  private anim: { obj: THREE.Object3D; from: THREE.Vector3; to: THREE.Vector3; t0: number; done: () => void } | null = null;
  private hl: Highlights = { selected: -1, targets: [], captures: [], last: [], check: -1 };

  constructor(private container: HTMLElement, private h: Handlers) {
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    this.renderer.shadowMap.enabled = true;
    this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    container.appendChild(this.renderer.domElement);
    this.scene.background = new THREE.Color(0xb4b4b4);

    this.scene.add(new THREE.HemisphereLight(0xffffff, 0x6a5a48, 1.1));
    const sun = new THREE.DirectionalLight(0xffffff, 1.6);
    sun.position.set(-4, 10, 6);
    sun.castShadow = true;
    sun.shadow.mapSize.set(2048, 2048);
    Object.assign(sun.shadow.camera, { left: -6, right: 6, top: 6, bottom: -6, near: 1, far: 30 });
    this.scene.add(sun);
    // a second light from the opposite (-z) side to even out the lighting; it casts no shadows so
    // pieces keep one clean shadow
    const fill = new THREE.DirectionalLight(0xffffff, 0.75);
    fill.position.set(-4, 10, -6);
    this.scene.add(fill);

    this.buildBoard();
    this.buildDropBox();
    this.buildLabels();
    this.scene.add(this.pieceRoot);
    this.setupControls();
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
      if (s >= 0) this.h.onSquare(s);
    });
    new ResizeObserver(() => this.resize()).observe(container);
    this.resize();
    requestAnimationFrame(this.frame);
  }

  private setupControls() {
    const el = this.renderer.domElement;
    // registered before DragControls so the pointer is current when its drag handler runs
    const track = (e: PointerEvent) => {
      const r = el.getBoundingClientRect();
      this.pointer.set(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
    };
    el.addEventListener("pointermove", (e) => {
      track(e);
      // a second button pressed while the first is held does not produce a pointerdown, only a
      // pointermove whose `buttons` now includes it: that is how the right click arrives mid-drag
      if (this.dragSq >= 0 && e.buttons & 2) this.cancelDrag();
    });
    el.addEventListener("pointerdown", (e) => {
      track(e);
      // a right press that does arrive as a pointerdown during a drag cancels it too
      if (e.button === 2 && this.dragSq >= 0) {
        e.stopImmediatePropagation();
        this.cancelDrag();
        return;
      }
      // DragControls only reacts to the primary button: right and middle belong to OrbitControls
      this.drag.enabled = e.button === 0;
    });
    el.addEventListener("pointerup", (e) => {
      track(e);
      this.drag.enabled = true;
    });
    // created before OrbitControls so its pointerdown runs first and can switch orbiting off
    this.drag = new DragControls(this.dragList, this.camera, el);
    this.drag.transformGroup = true; // move the whole piece, not the mesh that was hit
    this.orbit = new OrbitControls(this.camera, el);
    Object.assign(this.orbit, {
      enableZoom: false, rotateSpeed: 0.6, minPolarAngle: 0.25, maxPolarAngle: 1.05,
      enablePan: true, screenSpacePanning: false, // pan along the board, not up and down
    });
    // right drag rotates, middle drag pans; the left button is for pieces
    this.orbit.mouseButtons = { LEFT: null as any, MIDDLE: THREE.MOUSE.PAN, RIGHT: THREE.MOUSE.ROTATE };
    this.orbit.addEventListener("change", () => {
      // keep panning within a small range around the board centre (move camera and target together)
      const t = this.orbit.target;
      const dx = Math.max(-PAN_LIMIT, Math.min(PAN_LIMIT, t.x)) - t.x;
      const dz = Math.max(-PAN_LIMIT, Math.min(PAN_LIMIT, t.z)) - t.z;
      if (dx || dz) {
        t.x += dx;
        t.z += dz;
        this.camera.position.x += dx;
        this.camera.position.z += dz;
      }
      this.dirty = true;
    });

    this.drag.addEventListener("dragstart", (e: any) => {
      const sq = e.object.userData.sq as number;
      this.dragSq = this.h.canDrag(sq) ? sq : -1;
      if (this.dragSq >= 0) {
        this.orbit.enabled = false;
        this.h.onDragStart(sq);
      }
    });

    this.drag.addEventListener("drag", (e: any) => {
      const obj = e.object as THREE.Object3D;
      if (this.dragSq < 0) {
        // pieces that may not move stay put (and the gesture rotates the board instead)
        obj.position.copy(this.squarePos(obj.userData.sq));
        return;
      }
      this.followPointer(obj);
    });

    this.drag.addEventListener("dragend", (e: any) => {
      this.orbit.enabled = true;
      this.setDropTarget(-1);
      const from = this.dragSq;
      this.dragSq = -1;
      if (from < 0) return;
      const obj = e.object as THREE.Object3D;
      this.followPointer(obj);
      this.setDropTarget(-1); // followPointer may have shown the box again
      const to = this.squareAt(obj.position);
      if (to >= 0 && to !== from && this.h.onDrop(from, to)) {
        obj.position.copy(this.squarePos(to));
        this.dirty = true;
        return;
      }
      this.animateMove(from, from, () => {}); // not a legal drop: slide back to the start square
    });
  }

  // square index under a ground position, or -1 outside the board
  private squareAt(p: THREE.Vector3): number {
    const f = Math.round(p.x + 3.5), r = Math.round(3.5 - p.z);
    return f >= 0 && f < 8 && r >= 0 && r < 8 ? r * 8 + f : -1;
  }

  // put the piece being dragged back on its square. The left button may still be down, but the
  // piece no longer follows the pointer and nothing is played when it is released.
  private cancelDrag() {
    const from = this.dragSq;
    this.dragSq = -1;
    this.orbit.enabled = true;
    this.setDropTarget(-1);
    this.animateMove(from, from, () => {});
  }

  // the piece follows the pointer over the board (lifted a little, but its square is the ground point)
  private followPointer(obj: THREE.Object3D) {
    this.ray.setFromCamera(this.pointer, this.camera);
    const hit = new THREE.Vector3();
    if (this.ray.ray.intersectPlane(this.ground, hit)) {
      obj.position.set(Math.max(-4, Math.min(4, hit.x)), LIFT, Math.max(-4, Math.min(4, hit.z)));
      // highlight the square underneath if the held piece may legally go there
      const sq = this.squareAt(obj.position);
      this.setDropTarget(sq >= 0 && this.hl.targets.includes(sq) ? sq : -1);
    }
    this.dirty = true;
  }

  private squarePos(s: number) {
    return new THREE.Vector3(fileOf(s) - 3.5, 0, 3.5 - rankOf(s));
  }

  private buildBoard() {
    const frame = new THREE.Mesh(
      new THREE.BoxGeometry(FRAME_HALF * 2, 0.3, FRAME_HALF * 2),
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

  // a..h and 1..8 on the frame around the squares. Each label is a flat quad with a small
  // transparent text texture; updateLabels() moves them to the edges the viewer sees as bottom/right.
  private buildLabels() {
    const geo = new THREE.PlaneGeometry(LABEL_SIZE, LABEL_SIZE);
    const aniso = this.renderer.capabilities.getMaxAnisotropy();
    const make = (text: string) => {
      const cv = document.createElement("canvas");
      cv.width = cv.height = 128;
      const g = cv.getContext("2d")!;
      g.fillStyle = LABEL_COLOR;
      g.font = "bold 92px " + LABEL_FONT;
      g.textAlign = "center";
      g.textBaseline = "middle";
      g.fillText(text, 64, 70);
      const tex = new THREE.CanvasTexture(cv);
      tex.colorSpace = THREE.SRGBColorSpace;
      tex.anisotropy = aniso;
      const mesh = new THREE.Mesh(geo, new THREE.MeshBasicMaterial({ map: tex, transparent: true, depthWrite: false, toneMapped: false }));
      mesh.renderOrder = 1;
      this.scene.add(mesh);
      return mesh;
    };
    for (let copy = 0; copy < 2; copy++) {
      for (let i = 0; i < 8; i++) {
        this.fileLabels.push(make("abcdefgh"[i]));
        this.rankLabels.push(make(String(i + 1)));
      }
    }
  }

  // Painted on the frame like part of the board, so pieces in front of them hide them. Letters sit on
  // the near and far edges, numbers on the right and left edges, all reading upright for the viewer. Flipped (black at the bottom) the board is seen from the other side, so the
  // labels turn half a circle; the squares keep their world positions.
  private updateLabels() {
    const edge = LABEL_EDGE;
    const spin = this.flipped ? Math.PI : 0;
    for (let i = 0; i < 8; i++) {
      for (let copy = 0; copy < 2; copy++) {
        const side = copy === 0 ? 1 : -1;
        const f = this.fileLabels[i + 8 * copy];
        const r = this.rankLabels[i + 8 * copy];
        f.position.set(i - 3.5, 0.003, edge * side);
        r.position.set(edge * side, 0.003, 3.5 - i);
        f.rotation.set(-Math.PI / 2, 0, spin);
        r.rotation.set(-Math.PI / 2, 0, spin);
      }
    }
  }

  // a green square outline with a fixed width in screen pixels (Line2 draws it as screen-space quads)
  private buildDropBox() {
    const h = 0.47;
    const geo = new LineGeometry();
    geo.setPositions([-h, 0, -h, h, 0, -h, h, 0, h, -h, 0, h, -h, 0, -h]);
    const mat = new LineMaterial({
      color: DROP_COLOR, linewidth: DROP_WIDTH, worldUnits: false, transparent: true, opacity: DROP_OPACITY,
    });
    this.dropBox = new Line2(geo, mat);
    this.dropBox.position.y = 0.02;
    this.dropBox.visible = false;
    this.scene.add(this.dropBox);
  }

  // show the box on square s (or hide it with -1)
  private setDropTarget(s: number) {
    const was = this.dropBox.visible;
    this.dropBox.visible = s >= 0;
    if (s >= 0) {
      const p = this.squarePos(s);
      this.dropBox.position.set(p.x, 0.02, p.z);
    }
    if (was || s >= 0) this.dirty = true;
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
    this.dragList.length = 0;
    this.pieces.clear();
    for (let s = 0; s < 64; s++) {
      const p = board[s];
      if (!p) continue;
      const obj = makePiece(typeOf(p), colorOf(p) === WHITE);
      obj.position.copy(this.squarePos(s));
      obj.userData.sq = s;
      this.pieceRoot.add(obj);
      this.dragList.push(obj);
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
    this.updateLabels();
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
    this.renderer.setSize(w, h, false);
    this.dropBox.material.resolution.set(w, h);
    this.camera.aspect = w / h;
    // pull the camera back on narrow screens so the whole board stays visible
    const dist = Math.max(12.5, 16.5 / this.camera.aspect);
    const dir = new THREE.Vector3(0, 0.82, 0.58).normalize().multiplyScalar(dist);
    if (this.flipped) dir.z = -dir.z;
    const center = new THREE.Vector3(0, 0, this.flipped ? -VIEW_CENTER_Z : VIEW_CENTER_Z);
    this.camera.position.copy(dir).add(center);
    this.camera.lookAt(center);
    // allow a little rotation around the board's default viewing direction
    const base = this.flipped ? Math.PI : 0;
    this.orbit.minAzimuthAngle = base - 0.6;
    this.orbit.maxAzimuthAngle = base + 0.6;
    this.orbit.target.copy(center);
    this.orbit.update();
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
