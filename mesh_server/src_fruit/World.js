import * as THREE from "three";

export default class World {
  constructor() {
    const fov = 50;
    const aspect = window.innerWidth / window.innerHeight;
    const near = 10;
    const far = 20000;
    this.camera = new THREE.PerspectiveCamera(fov, aspect, near, far);
    this.camera.up.set(0, 1, 0);
    this.camera.position.set(0, 100, -4000);
    this.camera.lookAt(200, 0, 200);

    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0xeeeeee);

    this.meshes = [];
    this.images = [];

    const planeSize = [400, 250];

    this.ground = new THREE.Mesh(
      //unit is mm
      new THREE.PlaneGeometry(planeSize[0], planeSize[1]),
      new THREE.MeshPhongMaterial({ color: 0x666666, specular: 0x101010 })
    );
    this.ground.position.x = 0;
    this.ground.position.y = 0;
    this.ground.position.z = 0; // = - 0.5;
    this.ground.receiveShadow = true;
    //this.scene.add(this.ground);

    //lights
    this.scene.add(new THREE.HemisphereLight(0x333333, 0x111111));
    this.addShadowedLight(0, 1000, 100, 0xdddddd, 1.35, this.scene);
    this.addShadowedLight(200, -1000, 100, 0xcccccc, 1, this.scene);

    const defaultcol = 0xeeeeee;
    this.defaultMaterial = new THREE.MeshPhongMaterial({
      color: defaultcol,
      specular: defaultcol,
    });

    this.selectedInstance = [];
    this.geometries = [];

    this.instances = new THREE.Group();
    this.scene.add( this.instances);
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
