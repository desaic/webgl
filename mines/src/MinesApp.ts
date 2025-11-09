import * as THREE from "three";
import { OrbitControls } from "./OrbitControls.js";

import World from "./World.js";

import * as ImGui from "./utils/imgui/imgui";
import * as ImGui_Impl from "./utils/imgui/imgui_impl"

class MouseCoord {
  public x: number;
  public y: number;
  constructor() {
    this.x = 0;
    this.y = 0;
  }
};

export async function InitImGui() {
  if (ImGui) {
    await ImGui.default();
    // 2. Initialize ImGui
    ImGui.CreateContext();
  }
}

export class MinesApp {
  // HTML element that will contain the renderer's canvas
  private container: HTMLCanvasElement;

  // Three.js OrbitControls for camera manipulation
  private orbit: OrbitControls;

  // Three.js WebGL Renderer for rendering the 3D scene
  private renderer: THREE.WebGLRenderer;

  // meshes
  private world: World;

  private pixelRatio: number;

  public selectedList: HTMLElement;
  public isDragging = false;
  public startX = 0;
  public startY = 0;
  static DRAG_THRESHOLD = 5; // Minimum pixels moved to be considered a drag

  //global mouse xy coordinate
  public mouse_global: MouseCoord;

  public context2d: CanvasRenderingContext2D;
  public clear_color: ImGui.Vec4;

  constructor(canvas: HTMLCanvasElement) {
    if(!ImGui){
      console.log("imgui lib not found\n");
    }

    this.container = canvas;
    ImGui_Impl.Init(canvas);
    this.clear_color = new ImGui.Vec4(0.9,0.8,0.7,1);
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.world = new World();

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

  public render(time): void {
    this.renderer.render(this.world.scene, this.world.camera);    
    this.renderer.autoClear = false;


    ImGui_Impl.NewFrame(time);
    ImGui.NewFrame();
    
    if (ImGui.Begin("My First ImGui Window")) {
      // Display some text
      ImGui.Text("Hello from ImGui.js!");
      ImGui.Text("The quick brown fox jumps over the lazy dog.");

      ImGui.Text("Window is currently ACTIVE");
      
      // An interactive element
      if (ImGui.Button("Close Window")) {
        console.log("close");
      }

      // End the window call
      ImGui.End();
    }

    // -------------------------------------

    // 4. Finalize the ImGui frame and render
    ImGui.EndFrame();

    // Rendering commands
    ImGui.SetNextWindowBgAlpha(1.0);
    ImGui.Render();

    const gl = ImGui_Impl.gl;
    if(gl){
      gl.viewport(0, 0, gl.drawingBufferWidth, gl.drawingBufferHeight);
      gl.clearColor(this.clear_color.x, this.clear_color.y, this.clear_color.z, this.clear_color.w);
      gl.clear(gl.COLOR_BUFFER_BIT);
    }


    ImGui_Impl.RenderDrawData(ImGui.GetDrawData());
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
