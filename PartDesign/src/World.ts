import * as THREE from "three";
import {RectGrid} from "./RectGrid.js";

export default class World {
  public camera: THREE.PerspectiveCamera;
  public scene: THREE.Scene;
  public geometries: any[];
  public defaultMaterial: THREE.MeshPhongMaterial;
  public ground: RectGrid;
  public unitSphere :THREE.Mesh;
  public selectedInstance: any[];
  public instances: THREE.Group;
  public quadScene: THREE.Scene;
  public quadCamera: THREE.OrthographicCamera;
  public quadTexture: THREE.DataTexture;
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

    this.unitSphere = new THREE.Mesh(new THREE.IcosahedronGeometry(1, 50), this.defaultMaterial);
    this.unitSphere.scale.set(30,30,30);
    this.scene.add(this.unitSphere);

    this.selectedInstance = [];
    this.geometries = [];

    this.instances = new THREE.Group();
    this.scene.add(this.instances);


    // scene for rendering a single flat quad on top.
    this.quadScene = new THREE.Scene();
    const quad = this.MakeImageQuad();
    this.quadScene.add(quad);
    this.quadCamera=new THREE.OrthographicCamera(-aspect, aspect);
    this.quadCamera.position.set(0.2, 0.5, 5);
  }

  MakeImageQuad = ()=>{
    const width = 512; // Texture width
    const height = 512; // Texture height
    const size = width * height;
    const data = new Uint8Array(4 * size); // RGBA data
    
    // Generate a simple checkerboard pattern
    for (let i = 0; i < size; i++) {
      const stride = i * 4;
      const x = (i % width);
      const y = Math.floor(i / width);
      const color = (Math.floor(x/100) + Math.floor(y/100)) % 2 === 0 ? 255 : 50; // Black or white
      data[stride] = color;     // R
      data[stride + 1] = color; // G
      data[stride + 2] = color/2; // B
      data[stride + 3] = 255;   // A
    }
    
    const texture = new THREE.DataTexture(data, width, height);
    texture.needsUpdate = true;
    const material = new THREE.MeshBasicMaterial({ map: texture});
    const geometry = new THREE.PlaneGeometry(1, 1);
    const quad = new THREE.Mesh(geometry, material);
    quad.scale.set(0.8,0.8,0.8);
    this.quadTexture = texture;
    return quad;
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
