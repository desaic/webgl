import * as THREE from "three";

export default class World {
  public camera: THREE.OrthographicCamera;
  public scene: THREE.Scene;
  public parts: THREE.Group;
  constructor() {
    this.camera = new THREE.OrthographicCamera(0, 1, 1, 0, 0.1, 100);
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0xc0c0c0);
    this.parts = new THREE.Group();
    this.scene.add(this.parts);
    this.SetBoardSize(1, 1);
  }

  // One world unit per cell; board spans [0,cols] x [0,rows].
  SetBoardSize(cols: number, rows: number) {
    const cam = this.camera;
    cam.left = -cols / 2;
    cam.right = cols / 2;
    cam.top = rows / 2;
    cam.bottom = -rows / 2;
    cam.position.set(cols / 2, rows / 2, 5);
    cam.lookAt(cols / 2, rows / 2, 0);
    cam.updateProjectionMatrix();
  }

  Add(obj3d: THREE.Object3D) {
    this.parts.add(obj3d);
  }

  Remove(obj3d: THREE.Object3D) {
    this.parts.remove(obj3d);
  }
}
