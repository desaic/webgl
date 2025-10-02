import * as THREE from "three";
import {RectGrid} from "./RectGrid.js";

export default class World {
  public camera: THREE.PerspectiveCamera;
  public scene: THREE.Scene;
  // Parts should have their own types. using THREE.Mesh for now.
  // holds a single copy of the part's geometry so that instances
  // don't need to copy geometry.
  public parts: any[];
  public defaultMaterial: THREE.MeshPhongMaterial;
  public ground: RectGrid;
  public selectedInstance: any[];
  // instances of parts.
  public instances: THREE.Group;
  private nextPartId : number;
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
    this.scene.background = new THREE.Color(0xeeeeee);

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

    this.selectedInstance = [];
    this.parts = [];

    this.instances = new THREE.Group();
    this.scene.add(this.instances);

    this.nextPartId = 0;
  }

  GetInstanceById = (id) => {
    for (let i = 0; i < this.instances.children.length; i++) {
      const child = this.instances.children[i];
      if (child.id == id) {
        return child;
      }
    }
    return null;
  };

  GetPartById = (id) => {
    for (let i = 0; i < this.parts.length; i++) {
      if (this.parts[i].userData.id == id) {
        return this.parts[i];
      }
    }
    return null;
  };

  AddPart = (part:any)=>{
    const partId = this.nextPartId;
    this.nextPartId ++;
    part.userData.id = partId;
    this.parts.push(part);
    return partId;
  };

  // create an instance for partId, and add it to scene for rendering
  AddInstance = (pos: THREE.Vector3, rot:THREE.Euler, partId : number) => {
    const part=this.GetPartById(partId);
    if(part == null){
      return -1;
    }
    const instance = new THREE.Mesh(part.geometry, part.material.clone());
    instance.position.copy(pos);
    instance.setRotationFromEuler(rot);
    this.instances.add(instance);
    return instance.id;
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
