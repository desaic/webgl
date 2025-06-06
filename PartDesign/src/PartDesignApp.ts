import * as THREE from "three";
import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { OBJExporter } from "./OBJExporter.js";
import { GPUPicker } from "./gpupicker.js";
import { LoadMeshes } from './LoadMeshes';

export class PartDesignApp {
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

  private objExporter: OBJExporter;

  private pixelRatio: number;

  private loadingManager: THREE.LoadingManager;

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
    this.showSlice = false;
    this.loadingManager = new THREE.LoadingManager();
    this.objExporter = new OBJExporter();

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
    this.control.addEventListener("dragging-changed", function (event) {
      this.orbit.enabled = !event.value;
    });

    this.world.scene.add(this.control.getHelper());

    this.SetupLoadingManager();
    this.bindEventListeners();
  }

  public SetupLoadingManager(){
    this.loadingManager.onProgress=(url, loaded, number)=>{
      console.log(url);
      console.log(loaded);
      console.log(number);
    };
  }

  public bindEventListeners() {
    window.addEventListener("resize", () => this.onWindowResize());
    this.container.addEventListener("click", (event) => this.selectObj(event));

    const fileInput = document.getElementById("file-input") as HTMLInputElement;
    fileInput.addEventListener("change", (event) => {
      const target = event.target as HTMLInputElement;
      if (target.files.length > 0) this.LoadMeshes(target.files);
    });
  }
  /**
   * @method init
   * @description
   */
  public init(): void {}  

  public LoadMesh(file: File) {
    LoadMeshes(file, this.loadingManager);
  }

  public LoadMeshes(files: FileList) {
    if (!files) {
      return;
    }
    for (let i = 0; i < files.length; i++) {
      const file = files[i];
      this.LoadMesh(file);
    }
  }

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
