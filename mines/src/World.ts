import * as THREE from "three";

export default class World {
  public camera: THREE.PerspectiveCamera;
  public scene: THREE.Scene;
  public defaultMaterial: THREE.MeshPhongMaterial;
  public parts: THREE.Group;
  constructor() {
    const fov = 50;
    const aspect = window.innerWidth / window.innerHeight;
    const near = 0.05;
    const far = 50;
    this.camera = new THREE.PerspectiveCamera(fov, aspect, near, far);
    this.camera.position.set(0, 1.5, 4);
    this.camera.lookAt(0, 0, 0);

    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0xeeeeee);

    //lights
    this.scene.add(new THREE.HemisphereLight(0xdddddd, 0xbbbbbb));
    this.addShadowedLight(2, 1, 0,  0xcccccc, 1, this.scene);

    const defaultcol = 0xeeeeee;
    this.defaultMaterial = new THREE.MeshPhongMaterial({
      color: defaultcol,
      specular: defaultcol,
      side:THREE.DoubleSide
    });

    this.parts = new THREE.Group();
    this.scene.add(this.parts);
  }

  RemovePartByName = (name) => {
    this.parts.children = this.parts.children.filter(child => child.name != name);
  };

  Add(obj3d: THREE.Object3D){
    this.parts.add(obj3d);
  }

  addShadowedLight = (x, y, z, color, intensity, scene) => {
    const directionalLight = new THREE.DirectionalLight(color, intensity);
    directionalLight.position.set(x, y, z);
    scene.add(directionalLight);
    directionalLight.castShadow = true;
    const d = 400;
    directionalLight.shadow.camera.left = -d;
    directionalLight.shadow.camera.right = d;
    directionalLight.shadow.camera.top = d;
    directionalLight.shadow.camera.bottom = -d;
    directionalLight.shadow.camera.near = 1;
    directionalLight.shadow.camera.far = 400;
    directionalLight.shadow.bias = -0.002;
  };
}
