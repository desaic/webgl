import * as THREE from "three";
import {RectGrid} from "./RectGrid.js";

export default class World {
  constructor() {
    const fov = 50;
    const aspect = window.innerWidth / window.innerHeight;
    const near = 1;
    const far = 2000;
    this.camera = new THREE.PerspectiveCamera(fov, aspect, near, far);
    this.camera.up.set(0, 0, 1);
    this.camera.position.set(0, -400, 150);
    this.camera.lookAt(200, 125, 0);

    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x333333);

    this.meshes = [];

    const planeSize = [400, 250];

    this.ground = new RectGrid(40,20,10,0x666666, 0x444444);
    this.ground.setRotationFromEuler(new THREE.Euler(1.5708,0,0));
    this.ground.receiveShadow = true;
    this.scene.add(this.ground);

    //lights
    this.scene.add(new THREE.HemisphereLight(0x333333, 0x111111));
    this.addShadowedLight(0, 10, 100, 0xdddddd, 1.35, this.scene);
    this.addShadowedLight(200, 0, 100, 0xcccccc, 1, this.scene);

    const defaultcol = 0xeeeeee;
    this.defaultMaterial = new THREE.MeshPhongMaterial({
      color: defaultcol,
      specular: defaultcol,
    });

    
    
    this.unitSphere = new THREE.Mesh(new THREE.IcosahedronGeometry(100, 10), this.defaultMaterial);
    this.scene.add(this.unitSphere);

    this.selectedInstance = [];
    this.geometries = [];

    this.instances = new THREE.Group();
    this.scene.add(this.instances);
  }

  GetInstanceById = (id) => {
    for (let i = 0; i < this.instances.children.length; i++) {
      if (this.instances.children[i].id == id) {
        return this.instances.children[i];
      }
    }
    return null;
  };

  GetGeometryById = (id) => {
    for (let i = 0; i < this.geometries.length; i++) {
      if (this.geometries[i].id == id) {
        return this.geometries[i];
      }
    }
    return null;
  };

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
