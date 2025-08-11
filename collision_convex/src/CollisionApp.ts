import * as THREE from "three";
import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { GPUPicker } from "./gpupicker.js";
import { GUI } from 'lil-gui';
import { CuboidConf } from './CuboidConf';

export class CollisionApp {
  // HTML element that will contain the renderer's canvas
  private container: HTMLElement;

  // Three.js OrbitControls for camera manipulation
  private orbit: OrbitControls;

  // Three.js WebGL Renderer for rendering the 3D scene
  private renderer: THREE.WebGLRenderer;

  // meshes
  private world: World;

  private control: TransformControls;

  // GPU Picker for object selection (e.g., using a picking texture)
  private gpuPicker: any;

  // Flag to indicate whether a slice view should be shown
  private showSlice: boolean;

  private pixelRatio: number;

  private gui: GUI;
  private cuboidConfArr: CuboidConf[];
  /**
   * @constructor
   * @description
   */
  constructor(canvas: HTMLElement) {
    this.container = canvas;
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.world = new World();
    this.gpuPicker = new GPUPicker(
      THREE,
      this.renderer,
      this.world.scene,
      this.world.camera
    );
    this.gui = new GUI();
    const confA = new CuboidConf();
    const confB = new CuboidConf(); 
    confB.x = 4;
    this.cuboidConfArr = [confA, confB];
    this.SetupGUI();
    this.pixelRatio = window.devicePixelRatio ? window.devicePixelRatio : 1.0;

    this.renderer.setPixelRatio(this.pixelRatio);
    this.onWindowResize();
    this.container.appendChild(this.renderer.domElement);

    this.orbit = new OrbitControls(this.world.camera, this.renderer.domElement);
    this.orbit.minDistance = 2;
    this.orbit.maxDistance = 1000;
    this.orbit.maxPolarAngle = 3.13;
    this.orbit.minPolarAngle = 0.01;
    this.orbit.target.set(0, 0, -0.2);
    this.orbit.update();

    this.control = new TransformControls(
      this.world.camera,
      this.renderer.domElement
    );
    this.control.addEventListener("dragging-changed", (event) =>{
      this.orbit.enabled = !event.value;
    });

    this.world.scene.add(this.control.getHelper());

    this.AddCuboidsToScene();
    this.UpdateCuboidsRender();
    this.bindEventListeners();
  }
  
  public SetupGUI(){
    const folderA = this.gui.addFolder('Cuboic A');
    folderA.add(this.cuboidConfArr[0], 'x', -10,10,0.1 );
    folderA.add(this.cuboidConfArr[0], 'y', -10,10,0.1 );
    folderA.add(this.cuboidConfArr[0], 'z', -10,10,0.1 );
    folderA.add(this.cuboidConfArr[0], 'rx', -4,4,0.1 );
    folderA.add(this.cuboidConfArr[0], 'ry', -4,4,0.1 );
    folderA.add(this.cuboidConfArr[0], 'rz', -4,4,0.1 );
    const folderB = this.gui.addFolder('Cuboic B');
    folderB.add(this.cuboidConfArr[1], 'x', -10,10,0.1 );
    folderB.add(this.cuboidConfArr[1], 'y', -10,10,0.1 );
    folderB.add(this.cuboidConfArr[1], 'z', -10,10,0.1 );
    folderB.add(this.cuboidConfArr[1], 'rx', -4,4,0.1 );
    folderB.add(this.cuboidConfArr[1], 'ry', -4,4,0.1 );
    folderB.add(this.cuboidConfArr[1], 'rz', -4,4,0.1 );
  }

  public AddCuboidsToScene(){

  }

  public UpdateCuboidsRender(){

  }

  public AddObject3D(name: string, obj:THREE.Object3D){
    if(obj instanceof THREE.Mesh){
      console.log("adding mesh" + name);
      const partId = this.world.AddPart(obj);
      this.world.AddInstance(
        new THREE.Vector3(0, 0, 0),
        new THREE.Euler(0, 0, 0),
        partId
      );
    }else if(obj instanceof THREE.BufferGeometry){
      console.log('adding a BufferedGeometry ' + name);
      const mesh = new THREE.Mesh(obj, this.world.defaultMaterial);
      const partId = this.world.AddPart(mesh);
      this.world.AddInstance(
        new THREE.Vector3(0, 0, 0),
        new THREE.Euler(0, 0, 0),
        partId
      );
    }else{
      console.log('unknown mesh object type ' + name);
    }
  }

  public bindEventListeners() {
    window.addEventListener("resize", () => this.onWindowResize());
    this.container.addEventListener("click", (event) => this.selectObj(event));

  }
  /**
   * @method init
   * @description
   */
  public init(): void {}  

  /**
   * @method render
   * @description to be called in the main animation loop.
   */
  public render(): void {
    this.renderer.render(this.world.scene, this.world.camera);
    this.renderer.autoClear = false;
    if (this.showSlice) {
      this.renderer.render(this.world.quadScene, this.world.quadCamera);
    }
  }

  public onWindowResize(): void {
    const aspect = window.innerWidth / window.innerHeight;
    this.world.camera.aspect = aspect;
    this.world.camera.updateProjectionMatrix();

    const ocam = this.world.quadCamera;
    ocam.left = -aspect;
    ocam.right = aspect;
    ocam.updateProjectionMatrix();

    this.renderer.setSize(window.innerWidth, window.innerHeight);
  }

  public selectObj(event): void {
    var objId = this.gpuPicker.pick(
      event.clientX * this.pixelRatio,
      event.clientY * this.pixelRatio
    );
    if (objId < 0) {
      return;
    }
    console.log("pick", objId);
    const selected = this.world.GetInstanceById(objId);
    if (selected) {
      this.control.attach(selected);
    }
  }
}
