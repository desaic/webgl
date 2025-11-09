import * as THREE from "three";
import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { GPUPicker } from "./gpupicker.js";

class MouseCoord {
  public x: number;
  public y: number;
  constructor() {
    this.x = 0;
    this.y = 0;
  }
};

export class MinesApp {
  // HTML element that will contain the renderer's canvas
  private container: HTMLCanvasElement;

  // Three.js OrbitControls for camera manipulation
  private orbit: OrbitControls;

  // Three.js WebGL Renderer for rendering the 3D scene
  private renderer: THREE.WebGLRenderer;

  // meshes
  private world: World;

  private control: TransformControls;

  // GPU Picker for object selection (e.g., using a picking texture)
  private gpuPicker: any;

  private selection: string[];

  private pixelRatio: number;

  public selectedList: HTMLElement;
  public isDragging = false;
  public startX = 0;
  public startY = 0;
  static DRAG_THRESHOLD = 5; // Minimum pixels moved to be considered a drag

  //global mouse xy coordinate
  public mouse_global: MouseCoord;

  public context2d: CanvasRenderingContext2D;
  
  constructor(canvas: HTMLCanvasElement) {
    this.container = canvas;   
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.world = new World();
    this.gpuPicker = new GPUPicker(
      THREE,
      this.renderer,
      this.world.scene,
      this.world.camera
    );
    this.pixelRatio = window.devicePixelRatio ? window.devicePixelRatio : 1.0;

    this.renderer.setPixelRatio(this.pixelRatio);
    this.onWindowResize();
    this.container.appendChild(this.renderer.domElement);

    this.orbit = new OrbitControls(this.world.camera, this.renderer.domElement);
    this.orbit.minDistance = 0.1;
    this.orbit.maxDistance = 30;
    this.orbit.maxPolarAngle = 3.13;
    this.orbit.minPolarAngle = 0.01;
    this.orbit.target.set(0, 0, 0);
    this.orbit.update();

    this.bindEventListeners();
   
    this.selection = [];
    this.mouse_global = new MouseCoord();   
    this.SetupKbdEvents();
  }

  public SetupKbdEvents() {
    window.addEventListener("keydown", (event) => {
      // Ensure the key is an uppercase letter for case-insensitive check
      const key = event.key.toUpperCase();
      if (key == "H") {
        
      }
      else if (key == " ") {
      }
    });
  }

  public bindEventListeners() {
    window.addEventListener("resize", () => this.onWindowResize());
    
  }

  public onMouseMove(e) {
    this.mouse_global.x = e.clientX;
    this.mouse_global.y = e.clientY;

    // Only check if a mouse button is currently down (event.buttons is a bitmask)
    if (e.buttons === 0) {
      this.isDragging = false;
    } else {
      // Calculate distance moved
      const movedX = Math.abs(e.clientX - this.startX);
      const movedY = Math.abs(e.clientY - this.startY);

      if (
        movedX > MinesApp.DRAG_THRESHOLD ||
        movedY > MinesApp.DRAG_THRESHOLD
      ) {
        this.isDragging = true;
      }
    }

    if (this.isDragging) {
      return;
    }

  }

  public render(): void {
    this.renderer.render(this.world.scene, this.world.camera);    
    this.renderer.autoClear = false;
  }

  public onWindowResize(): void {
    const width = this.container.clientWidth;
    const height = this.container.clientHeight;
    const aspect = width / height;
    this.world.camera.aspect = aspect;
    this.world.camera.updateProjectionMatrix();

    this.renderer.setSize(width, height);
  }

  public GetCanvasCoord(x: number, y: number): MouseCoord {
    const canvas = this.container;
    if (!canvas) {
      return { x: x, y: y };
    }

    // 3. Get the bounding rectangle of the canvas
    // This is the most robust way, as it accounts for both the left offset
    // (due to the side panel) and any top/margin offsets.
    const canvasRect = canvas.getBoundingClientRect();
    return { x: x - canvasRect.left, y: y - canvasRect.top };
  }

}
