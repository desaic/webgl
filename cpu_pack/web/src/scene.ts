import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { TransformControls } from "three/examples/jsm/controls/TransformControls.js";
import { OBJLoader } from "three/examples/jsm/loaders/OBJLoader.js";
import { STLLoader } from "three/examples/jsm/loaders/STLLoader.js";
import { fetchBuffer } from "./api";
import { Inst } from "./parse";
import { Contact, collisions } from "./collide";
import { Culler, decimate } from "./cull";

export interface Item {
  inst: Inst;
  mesh: THREE.Mesh;
  color: THREE.Color;
  visible: boolean;
  radius: number;
  full: THREE.BufferGeometry;
  edited: boolean;
}

// unique instance name, same scheme as Blender's load_pack.py (name_index)
export const labelOf = (i: Inst) => i.name + "_" + i.id;

function hashHue(s: string): number {
  let h = 2166136261;
  for (let i = 0; i < s.length; i++) h = Math.imul(h ^ s.charCodeAt(i), 16777619);
  return ((h >>> 0) % 360) / 360;
}

export class Viewer {
  renderer = new THREE.WebGLRenderer({ antialias: true, preserveDrawingBuffer: true });
  scene = new THREE.Scene();
  camera = new THREE.PerspectiveCamera(40, 1, 0.1, 100000);
  controls: OrbitControls;
  fruits = new THREE.Group();
  containers = new THREE.Group();
  items: Item[] = [];
  selected: Item | null = null;
  clipPlane = new THREE.Plane(new THREE.Vector3(0, 0, -1), 0);
  clipOn = false;
  clipAxis = 2;
  clipFlip = false;
  fruitOpacity = 1;
  contOpacity = 0.25;
  wire = false;
  contWire = false;
  step = 0;
  maxStep = 0;
  onPick: (it: Item | null) => void = () => {};
  onCamera: () => void = () => {};
  onEdit: (it: Item) => void = () => {};
  tc: TransformControls;

  private poseMat = new THREE.Matrix4();
  private geoCache = new Map<string, Promise<THREE.BufferGeometry>>();
  private objLoader = new OBJLoader();
  private stlLoader = new STLLoader();
  private labelEls: HTMLDivElement[] = [];
  private labelsOn = false;
  private host: HTMLElement;
  private dirty = true;
  culler = new Culler();
  cullOn = true;
  cullInfo = "";
  lodPx = 70; // fruit whose bounding sphere is smaller than this on screen use a coarse mesh
  lodCells = 16;
  private lowGeo = new Map<THREE.BufferGeometry, THREE.BufferGeometry>();

  constructor(host: HTMLElement) {
    this.host = host;
    host.appendChild(this.renderer.domElement);
    this.renderer.localClippingEnabled = true;
    this.renderer.setClearColor(0x1b1d22);
    this.camera.up.set(0, 0, 1); // same Z-up convention as Blender
    this.camera.position.set(300, -300, 200);
    this.controls = new OrbitControls(this.camera, this.renderer.domElement);
    this.controls.enableDamping = false;
    // zoom toward the point under the cursor instead of the orbit target, so zooming in does not
    // slow to a crawl, and let the target follow so panning speed matches what is on screen.
    this.controls.zoomToCursor = true;
    this.controls.zoomSpeed = 1.5;
    this.controls.minDistance = 1;
    this.controls.screenSpacePanning = true;
    // pan and zoom speed scale with the distance from the camera to the orbit target, so keep that
    // distance equal to the depth of the surface under the cursor (like Blender's auto depth).
    const dom = this.renderer.domElement;
    dom.addEventListener("pointerdown", (e) => this.retarget(e.clientX, e.clientY), true);
    dom.addEventListener("wheel", (e) => this.retarget(e.clientX, e.clientY), true);
    this.controls.addEventListener("change", () => {
      this.dirty = true;
      this.onCamera();
    });
    // translate + rotate only, no scaling
    this.tc = new TransformControls(this.camera, this.renderer.domElement);
    this.tc.setMode("translate");
    this.tc.addEventListener("dragging-changed", (e) => {
      this.controls.enabled = !(e as any).value;
      this.dirty = true;
    });
    this.tc.addEventListener("change", () => (this.dirty = true));
    this.tc.addEventListener("objectChange", () => {
      if (this.selected) {
        this.writePose(this.selected);
        this.onEdit(this.selected);
      }
    });
    this.scene.add(this.tc.getHelper());
    this.scene.add(new THREE.HemisphereLight(0xffffff, 0x404050, 1.1));
    const dl = new THREE.DirectionalLight(0xffffff, 1.6);
    this.camera.add(dl);
    dl.position.set(0.4, 0.6, 1);
    this.scene.add(this.camera);
    this.scene.add(this.containers, this.fruits);
    window.addEventListener("resize", () => this.resize());
    new ResizeObserver(() => this.resize()).observe(host);
    this.resize();
    this.bindPick();
    const loop = () => {
      requestAnimationFrame(loop);
      if (this.dirty) {
        this.dirty = false;
        this.cull();
        this.renderer.render(this.scene, this.camera);
        this.updateLabels();
      }
    };
    loop();
  }

  // hide fruit outside the view or behind occupied cells. the user hide flag stays in Item.visible.
  private cull() {
    const items = this.items;
    if (!items.length) return;
    if (!this.cullOn) {
      for (const it of items) {
        it.mesh.visible = it.visible;
        if (it.mesh.geometry !== it.full) it.mesh.geometry = it.full;
      }
      this.cullInfo = "";
      return;
    }
    // sections cut through fruit and see-through fruit show what is behind, so only frustum culling is valid
    const occ = !this.clipOn && this.fruitOpacity >= 1 && !this.wire;
    const draw = this.culler.run(
      items.map((it) => ({
        pos: it.mesh.position,
        radius: it.radius * it.mesh.scale.x,
        keep: it === this.selected,
      })),
      this.camera,
      occ
    );
    const camPos = this.camera.position;
    const pxPerUnit = this.renderer.domElement.clientHeight / (2 * Math.tan((this.camera.fov * Math.PI) / 360));
    let low = 0;
    items.forEach((it, i) => {
      it.mesh.visible = it.visible && draw[i];
      if (!it.mesh.visible) return;
      const d = Math.max(1e-3, it.mesh.position.distanceTo(camPos));
      const coarse = it !== this.selected && (it.radius * it.mesh.scale.x * pxPerUnit) / d < this.lodPx;
      let g = it.full;
      if (coarse) {
        let lg = this.lowGeo.get(it.full);
        if (!lg) {
          lg = decimate(it.full, this.lodCells);
          this.lowGeo.set(it.full, lg);
        }
        g = lg;
        low++;
      }
      if (it.mesh.geometry !== g) it.mesh.geometry = g;
    });
    const s = this.culler.stats;
    this.cullInfo = "drawn " + s.items + " / " + s.total + ", coarse " + low;
    const el = document.getElementById("cullInfo");
    if (el) el.textContent = this.cullInfo;
  }

  redraw() {
    this.dirty = true;
  }

  resize() {
    const w = Math.max(1, this.host.clientWidth);
    const h = Math.max(1, this.host.clientHeight);
    this.renderer.setPixelRatio(window.devicePixelRatio);
    this.renderer.setSize(w, h, false);
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
    this.dirty = true;
  }

  // ---- geometry loading ----
  loadGeometry(path: string): Promise<THREE.BufferGeometry> {
    let p = this.geoCache.get(path);
    if (!p) {
      p = this.readGeometry(path);
      this.geoCache.set(path, p);
      p.catch(() => this.geoCache.delete(path));
    }
    return p;
  }

  clearGeometryCache() {
    this.geoCache.clear();
    this.lowGeo.clear();
  }

  private async readGeometry(path: string): Promise<THREE.BufferGeometry> {
    const buf = await fetchBuffer(path);
    let geo: THREE.BufferGeometry;
    if (/\.stl$/i.test(path)) {
      geo = this.stlLoader.parse(buf);
    } else {
      // stray "l"/"p" records make OBJLoader turn the whole object into lines
      const text = new TextDecoder().decode(buf).replace(/^[lp][ \t].*$/gm, "");
      const obj = this.objLoader.parse(text);
      const parts: THREE.BufferGeometry[] = [];
      obj.traverse((o) => {
        if ((o as THREE.Mesh).isMesh) {
          const g = (o as THREE.Mesh).geometry.clone();
          g.applyMatrix4(o.matrixWorld);
          parts.push(g);
        }
      });
      geo = mergeGeos(parts);
    }
    if (!geo.getAttribute("normal")) geo.computeVertexNormals();
    return geo;
  }

  // ---- materials ----
  private clipList(): THREE.Plane[] {
    return this.clipOn ? [this.clipPlane] : [];
  }

  private fruitMaterial(color: THREE.Color): THREE.MeshStandardMaterial {
    return new THREE.MeshStandardMaterial({
      color,
      roughness: 0.6,
      metalness: 0.05,
      side: THREE.DoubleSide,
      wireframe: this.wire,
      transparent: this.fruitOpacity < 1,
      opacity: this.fruitOpacity,
      clippingPlanes: this.clipList(),
    });
  }

  private contMaterial(): THREE.MeshStandardMaterial {
    return new THREE.MeshStandardMaterial({
      color: 0xd9b8a0,
      roughness: 0.8,
      side: THREE.DoubleSide,
      wireframe: this.contWire,
      transparent: true,
      opacity: this.contOpacity,
      depthWrite: this.contOpacity >= 0.99,
      clippingPlanes: this.clipList(),
    });
  }

  // ---- containers ----
  async addContainer(path: string, replace: boolean) {
    const geo = await this.loadGeometry(path);
    if (replace) this.clearContainers();
    const m = new THREE.Mesh(geo, this.contMaterial());
    m.userData.container = true;
    m.renderOrder = 1;
    this.containers.add(m);
    this.dirty = true;
  }

  clearContainers() {
    this.containers.clear();
    this.dirty = true;
  }

  // ---- instances ----
  async setInstances(insts: Inst[], pathFor: (i: Inst) => string, onProgress?: (n: number, total: number) => void) {
    this.tc.detach();
    this.fruits.clear();
    this.items = [];
    this.selected = null;
    const uniq = [...new Set(insts.map(pathFor))];
    let done = 0;
    const failed: string[] = [];
    await Promise.all(
      uniq.map((p) =>
        this.loadGeometry(p)
          .catch(() => failed.push(p))
          .finally(() => onProgress && onProgress(++done, uniq.length))
      )
    );
    for (const inst of insts) {
      const p = pathFor(inst);
      if (failed.includes(p)) continue;
      this.makeItem(inst, await this.loadGeometry(p));
    }
    this.maxStep = Math.max(0, ...insts.map((i) => i.poses.length - 1));
    this.setStep(Math.min(this.step, this.maxStep));
    this.setClipRange();
    this.dirty = true;
    return failed;
  }

  private makeItem(inst: Inst, geo: THREE.BufferGeometry): Item {
    const color = new THREE.Color().setHSL(hashHue(inst.name), 0.55, 0.55);
    const mesh = new THREE.Mesh(geo, this.fruitMaterial(color));
    const item: Item = { inst, mesh, color, visible: true, radius: 0, full: geo, edited: false };
    if (!geo.boundingSphere) geo.computeBoundingSphere();
    item.radius = geo.boundingSphere!.radius;
    mesh.userData.item = item;
    this.applyPose(item);
    this.items.push(item);
    this.fruits.add(mesh);
    return item;
  }

  // pose rotation is stored row-major, same as Blender Matrix((row0), ...)
  private applyPose(it: Item) {
    const p = it.inst.poses[Math.min(this.step, it.inst.poses.length - 1)];
    const r = p.rot;
    const m = this.poseMat;
    m.set(
      r[0], r[1], r[2], p.pos[0],
      r[3], r[4], r[5], p.pos[1],
      r[6], r[7], r[8], p.pos[2],
      0, 0, 0, 1
    );
    m.decompose(it.mesh.position, it.mesh.quaternion, it.mesh.scale);
    it.mesh.scale.setScalar(p.scale);
  }

  // store the gizmo-edited transform back into the instance pose
  private writePose(it: Item) {
    const p = it.inst.poses[Math.min(this.step, it.inst.poses.length - 1)];
    const e = this.poseMat.makeRotationFromQuaternion(it.mesh.quaternion).elements;
    for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) p.rot[i * 3 + j] = e[j * 4 + i];
    p.pos = [it.mesh.position.x, it.mesh.position.y, it.mesh.position.z];
    it.edited = true;
  }

  setStep(s: number) {
    this.step = s;
    for (const it of this.items) this.applyPose(it);
    this.dirty = true;
  }

  // ---- editing ----
  private idCounter = 0;

  // one global increasing integer N, so that name_N is not used by any instance
  private nextId(name: string): number {
    const used = new Set(this.items.map((i) => labelOf(i.inst)));
    for (const i of this.items) this.idCounter = Math.max(this.idCounter, i.inst.id + 1);
    while (used.has(name + "_" + this.idCounter)) this.idCounter++;
    return this.idCounter++;
  }

  async spawn(name: string, path: string, at: THREE.Vector3): Promise<Item> {
    const geo = await this.loadGeometry(path);
    const inst: Inst = {
      id: this.nextId(name),
      name,
      meshPath: path,
      poses: [{ pos: [at.x, at.y, at.z], rot: [1, 0, 0, 0, 1, 0, 0, 0, 1], scale: 1 }],
    };
    const item = this.makeItem(inst, geo);
    item.edited = true;
    this.select(item);
    this.dirty = true;
    return item;
  }

  // duplicate an instance at its current pose, nudged sideways so it is visible
  copyItem(src: Item): Item {
    const sp = src.inst.poses[Math.min(this.step, src.inst.poses.length - 1)];
    const g = src.mesh.geometry;
    if (!g.boundingSphere) g.computeBoundingSphere();
    const right = new THREE.Vector3().setFromMatrixColumn(this.camera.matrixWorld, 0);
    const off = right.multiplyScalar(g.boundingSphere.radius * sp.scale * 0.6);
    const inst: Inst = {
      id: this.nextId(src.inst.name),
      name: src.inst.name,
      meshPath: src.inst.meshPath,
      poses: [{ pos: [sp.pos[0] + off.x, sp.pos[1] + off.y, sp.pos[2] + off.z], rot: [...sp.rot], scale: sp.scale }],
    };
    const item = this.makeItem(inst, g);
    item.edited = true;
    this.select(item);
    this.dirty = true;
    return item;
  }

  removeItem(it: Item) {
    if (this.selected === it) this.select(null);
    this.fruits.remove(it.mesh);
    (it.mesh.material as THREE.Material).dispose();
    this.items.splice(this.items.indexOf(it), 1);
    this.dirty = true;
  }

  // contacting neighbors and sampled penetration depth of one fruit
  contacts(it: Item): Contact[] {
    const fruits = this.items.filter((i) => i.visible).map((i) => ({ label: labelOf(i.inst), mesh: i.mesh }));
    return collisions(it.mesh, fruits, this.containers.children as THREE.Mesh[]);
  }

  setGizmoMode(mode: "translate" | "rotate") {
    this.tc.setMode(mode);
    this.dirty = true;
  }

  setGizmoSpace(space: "local" | "world") {
    this.tc.setSpace(space);
    this.dirty = true;
  }

  exportPack(): string {
    const f = (v: number) => String(Math.round(v * 1e6) / 1e6);
    const lines = [String(this.items.length)];
    for (const it of this.items) {
      const p = it.inst.poses[Math.min(this.step, it.inst.poses.length - 1)];
      lines.push([it.inst.name, "pos", ...p.pos.map(f), "rot", ...p.rot.map(f), "scale", f(p.scale)].join(" "));
    }
    return lines.join("\n") + "\n";
  }

  // ---- appearance ----
  applyMaterials() {
    for (const it of this.items) {
      const old = it.mesh.material as THREE.Material;
      old.dispose();
      it.mesh.material = this.fruitMaterial(it.color);
    }
    for (const c of this.containers.children as THREE.Mesh[]) {
      (c.material as THREE.Material).dispose();
      c.material = this.contMaterial();
    }
    this.refreshHighlight();
    this.dirty = true;
  }

  setLabels(on: boolean) {
    this.labelsOn = on;
    if (!on) {
      this.labelEls.forEach((e) => e.remove());
      this.labelEls = [];
    }
    this.dirty = true;
  }

  private updateLabels() {
    if (!this.labelsOn) return;
    while (this.labelEls.length < this.items.length) {
      const d = document.createElement("div");
      d.className = "label";
      this.host.appendChild(d);
      this.labelEls.push(d);
    }
    const v = new THREE.Vector3();
    const w = this.host.clientWidth;
    const h = this.host.clientHeight;
    this.items.forEach((it, i) => {
      const el = this.labelEls[i];
      v.copy(it.mesh.position).project(this.camera);
      const show = it.visible && v.z < 1 && Math.abs(v.x) < 1.05 && Math.abs(v.y) < 1.05;
      el.style.display = show ? "block" : "none";
      el.textContent = labelOf(it.inst);
      el.style.left = ((v.x + 1) * 0.5 * w) + "px";
      el.style.top = ((1 - v.y) * 0.5 * h) + "px";
    });
  }

  // ---- visibility / selection ----
  setVisible(it: Item, vis: boolean) {
    it.visible = vis;
    it.mesh.visible = vis;
    if (this.selected === it) this.syncGizmo();
    this.dirty = true;
  }

  isolate(it: Item) {
    for (const o of this.items) this.setVisible(o, o === it);
  }

  showAll() {
    for (const o of this.items) this.setVisible(o, true);
  }

  select(it: Item | null) {
    this.selected = it;
    this.syncGizmo();
    this.refreshHighlight();
    this.onPick(it);
  }

  private syncGizmo() {
    if (this.selected && this.selected.visible) this.tc.attach(this.selected.mesh);
    else this.tc.detach();
  }

  private refreshHighlight() {
    for (const o of this.items) {
      const m = o.mesh.material as THREE.MeshStandardMaterial;
      m.emissive.set(o === this.selected ? 0x445577 : 0x000000);
    }
    this.dirty = true;
  }

  private bindPick() {
    const el = this.renderer.domElement;
    let down: [number, number] | null = null;
    let onGizmo = false;
    // capture phase: TransformControls clears its hovered axis on its own pointerup
    el.addEventListener("pointerdown", (e) => {
      down = [e.clientX, e.clientY];
      onGizmo = this.tc.axis !== null;
    }, true);
    el.addEventListener("pointerup", (e) => {
      if (!down) return;
      if (onGizmo) {
        down = null;
        return;
      }
      const moved = Math.hypot(e.clientX - down[0], e.clientY - down[1]);
      down = null;
      if (moved > 4) return;
      const r = el.getBoundingClientRect();
      const ndc = new THREE.Vector2(((e.clientX - r.left) / r.width) * 2 - 1, -((e.clientY - r.top) / r.height) * 2 + 1);
      const ray = new THREE.Raycaster();
      ray.setFromCamera(ndc, this.camera);
      const meshes = this.items.filter((i) => i.mesh.visible).map((i) => i.mesh);
      const hits = ray.intersectObjects(meshes, false).filter((h) => !this.clippedOut(h.point));
      this.select(hits.length ? (hits[0].object.userData.item as Item) : null);
    });
  }

  // move the orbit target along the view axis to the depth of the fruit under the cursor. the view
  // direction does not change, so nothing jumps; only the speed of pan and zoom is rescaled.
  private retarget(clientX: number, clientY: number) {
    const r = this.renderer.domElement.getBoundingClientRect();
    const ndc = new THREE.Vector2(((clientX - r.left) / r.width) * 2 - 1, -((clientY - r.top) / r.height) * 2 + 1);
    const ray = new THREE.Raycaster();
    ray.setFromCamera(ndc, this.camera);
    const meshes = this.items.filter((i) => i.mesh.visible).map((i) => i.mesh);
    const hit = ray.intersectObjects(meshes, false).find((h) => !this.clippedOut(h.point));
    if (!hit) return;
    const fwd = this.camera.getWorldDirection(new THREE.Vector3());
    const depth = hit.point.clone().sub(this.camera.position).dot(fwd);
    if (depth < 0.5) return;
    this.controls.target.copy(this.camera.position).addScaledVector(fwd, depth);
  }

  private clippedOut(p: THREE.Vector3): boolean {
    return this.clipOn && this.clipPlane.distanceToPoint(p) < 0;
  }

  // ---- clipping ----
  bounds(): THREE.Box3 {
    const b = new THREE.Box3();
    this.containers.children.forEach((c) => {
      const g = (c as THREE.Mesh).geometry;
      if (!g.boundingBox) g.computeBoundingBox();
      b.union(g.boundingBox);
    });
    for (const it of this.items) {
      const g = it.mesh.geometry;
      if (!g.boundingBox) g.computeBoundingBox();
      it.mesh.updateMatrix();
      b.union(g.boundingBox.clone().applyMatrix4(it.mesh.matrix));
    }
    return b;
  }

  clipValue = 0.5;

  setClipRange() {
    this.updateClip();
  }

  updateClip() {
    const b = this.bounds();
    if (b.isEmpty()) return;
    const ax = ["x", "y", "z"][this.clipAxis] as "x" | "y" | "z";
    const pos = b.min[ax] + (b.max[ax] - b.min[ax]) * this.clipValue;
    const n = new THREE.Vector3();
    n.setComponent(this.clipAxis, this.clipFlip ? 1 : -1);
    this.clipPlane.normal.copy(n);
    this.clipPlane.constant = this.clipFlip ? -pos : pos;
    this.applyMaterials();
  }

  // ---- camera ----
  fit(target?: THREE.Box3) {
    const b = target || this.bounds();
    if (b.isEmpty()) return;
    const c = b.getCenter(new THREE.Vector3());
    const r = Math.max(1e-3, b.getSize(new THREE.Vector3()).length() * 0.5);
    const dir = this.camera.position.clone().sub(this.controls.target);
    if (dir.lengthSq() < 1e-9) dir.set(1, -1, 0.7);
    dir.normalize();
    const dist = r / Math.sin((this.camera.fov * Math.PI) / 360);
    this.camera.position.copy(c).addScaledVector(dir, dist * 1.05);
    this.camera.near = dist / 1000;
    this.camera.far = dist * 100;
    this.camera.updateProjectionMatrix();
    this.controls.target.copy(c);
    this.controls.update();
    this.dirty = true;
  }

  focus(it: Item) {
    const g = it.mesh.geometry;
    if (!g.boundingBox) g.computeBoundingBox();
    it.mesh.updateMatrix();
    this.fit(g.boundingBox.clone().applyMatrix4(it.mesh.matrix).expandByScalar(5));
  }

  getCamera(): number[] {
    const p = this.camera.position;
    const t = this.controls.target;
    return [p.x, p.y, p.z, t.x, t.y, t.z].map((v) => Math.round(v * 100) / 100);
  }

  setCamera(c: number[]) {
    this.camera.position.set(c[0], c[1], c[2]);
    this.controls.target.set(c[3], c[4], c[5]);
    const d = this.camera.position.distanceTo(this.controls.target);
    this.camera.near = d / 1000;
    this.camera.far = d * 100;
    this.camera.updateProjectionMatrix();
    this.controls.update();
    this.dirty = true;
  }

  screenshot(): Promise<Blob | null> {
    this.renderer.render(this.scene, this.camera);
    return new Promise((res) => this.renderer.domElement.toBlob(res, "image/png"));
  }
}

function mergeGeos(parts: THREE.BufferGeometry[]): THREE.BufferGeometry {
  if (parts.length === 0) throw new Error("obj has no meshes");
  if (parts.length === 1) return parts[0];
  let n = 0;
  for (const p of parts) n += p.getAttribute("position").count;
  const pos = new Float32Array(n * 3);
  let o = 0;
  for (const p of parts) {
    const a = p.getAttribute("position");
    for (let i = 0; i < a.count; i++) {
      pos[o++] = a.getX(i);
      pos[o++] = a.getY(i);
      pos[o++] = a.getZ(i);
    }
  }
  const g = new THREE.BufferGeometry();
  g.setAttribute("position", new THREE.BufferAttribute(pos, 3));
  g.computeVertexNormals();
  return g;
}
