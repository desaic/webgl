import * as THREE from "three";
import { OrbitControls } from "three/examples/jsm/controls/OrbitControls.js";
import { ViewCube } from "./ViewCube.js";

export class ViewerApp {
  constructor(canvas) {
    this.canvas = canvas;

    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
    this.renderer.setPixelRatio(window.devicePixelRatio);

    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x1a1a1a);

    this.camera = new THREE.PerspectiveCamera(
      45,
      1,
      0.1,
      10000
    );
    // Z is up: tell the camera + OrbitControls to treat +Z as the up axis.
    this.camera.up.set(0, 0, 1);
    this.camera.position.set(7, -7, 5);

    this.controls = new OrbitControls(this.camera, canvas);

    this.viewCube = new ViewCube(this.camera, canvas, this.controls);

    // Click handler: intercept clicks on the ViewCube before OrbitControls
    // gets them (capture phase + stopPropagation).
    this._onPointerDown = (e) => {
      if (this.viewCube.handleClick(e)) {
        e.stopPropagation();
      }
    };
    canvas.addEventListener("pointerdown", this._onPointerDown, true);

    // Single shared default material reused across all loaded models.
    this.defaultMaterial = new THREE.MeshStandardMaterial({
      color: 0x9bb4c4,
      metalness: 0.1,
      roughness: 0.6,
      flatShading: false,
    });

    this.currentModel = null;

    this._addLights();
    this._addHelpers();

    // Size the renderer to the canvas's current CSS size, then keep it in
    // sync via ResizeObserver so sidebar drags / window resizes all flow
    // through one path.
    this._resize();
    this._resizeObserver = new ResizeObserver(() => this._resize());
    this._resizeObserver.observe(this.canvas);
  }

  _addLights() {
    const ambient = new THREE.AmbientLight(0xffffff, 0.6);
    this.scene.add(ambient);

    const dir = new THREE.DirectionalLight(0xffffff, 1.0);
    dir.position.set(10, 20, 15);
    this.scene.add(dir);

    const dir2 = new THREE.DirectionalLight(0xffffff, 0.4);
    dir2.position.set(-10, 5, -15);
    this.scene.add(dir2);
  }

  _addHelpers() {
    const grid = new THREE.GridHelper(20, 20, 0x444444, 0x2a2a2a);
    // GridHelper is built in the XZ plane (Y up); rotate it into the XY
    // plane so it sits on the ground with Z as the up axis.
    grid.rotation.x = -Math.PI / 2;
    this.scene.add(grid);
  }

  clearModel() {
    if (!this.currentModel) return;
    this.scene.remove(this.currentModel);
    this.currentModel.traverse((obj) => {
      if (obj.geometry) obj.geometry.dispose();
      // Don't dispose the shared defaultMaterial.
    });
    this.currentModel = null;
  }

  setModel(object) {
    this.clearModel();
    this.scene.add(object);
    this.currentModel = object;
    this.fitCameraToObject(object);
  }

  fitCameraToObject(object) {
    const box = new THREE.Box3().setFromObject(object);
    const size = box.getSize(new THREE.Vector3());
    const center = box.getCenter(new THREE.Vector3());

    const maxDim = Math.max(size.x, size.y, size.z) || 1;
    const fov = (this.camera.fov * Math.PI) / 180;
    let distance = (maxDim / 2) / Math.tan(fov / 2);
    distance *= 1.6; // pad a bit

    const dirVec = new THREE.Vector3(1, -0.8, 0.6).normalize();
    this.camera.position.copy(center).addScaledVector(dirVec, distance);
    this.camera.near = Math.max(distance / 100, 0.01);
    this.camera.far = distance * 100;
    this.camera.updateProjectionMatrix();

    this.controls.target.copy(center);
    this.controls.update();
  }

  _resize() {
    const w = this.canvas.clientWidth || 1;
    const h = this.canvas.clientHeight || 1;
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();
    // updateStyle=false: let CSS (width/height:100%) control display size,
    // only update the drawing buffer. Avoids ResizeObserver feedback loops.
    this.renderer.setSize(w, h, false);
    // Resizing the drawing buffer clears it to black; re-render immediately
    // so the browser never composites an empty buffer (prevents flicker).
    this.renderer.render(this.scene, this.camera);
  }

  start() {
    const clock = new THREE.Clock();
    const animate = () => {
      requestAnimationFrame(animate);
      const delta = clock.getDelta();
      this.viewCube.update(delta);
      this.controls.update();
      this.renderer.render(this.scene, this.camera);
      this.viewCube.render(this.renderer);
    };
    animate();
  }
}
