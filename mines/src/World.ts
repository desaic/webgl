import * as THREE from "three";

export default class World {
  public camera: THREE.OrthographicCamera;
  public scene: THREE.Scene;
  public parts: THREE.Group;
  public cameraSize: number;
  constructor() {
    this.cameraSize = 10;
    this.camera = new THREE.OrthographicCamera(0, this.cameraSize, this.cameraSize, 0, 0.1, 100);
    this.camera.position.set(this.cameraSize / 2, this.cameraSize / 2, 5);
    this.camera.lookAt(0, 0, 0);
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0xeeeeee);
    //lights
    this.scene.add(new THREE.HemisphereLight(0xdddddd, 0xbbbbbb));
    this.parts = new THREE.Group();
    this.scene.add(this.parts);
  }

  Add(obj3d: THREE.Object3D) {
    this.parts.add(obj3d);
  }
}
