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
    const io = ImGui.GetIO();
    
    // --- STEP 3A: Load the Font ---
    // The path is relative to the *runtime environment* (usually the root of your web server)
    const fontPath = "./assets/Montserrat-Regular.ttf"; 
    const fontSize = 15.0; 
    let fontData;
    try {
      const response = await fetch(fontPath);
      if (!response.ok) {
        throw new Error(`HTTP error! status: ${response.status}`);
      }

      // Get the font data as an ArrayBuffer
      fontData = await response.arrayBuffer();
    } catch (error) {
      console.error("Failed to fetch or load Montserrat font:", error);
    }
    if(fontData){
      // AddFontFromFileTTF returns an ImFont pointer (object)
      const myMontserratFont = io.Fonts.AddFontFromMemoryTTF(fontData, fontSize);
      if (myMontserratFont) {
          // Optional: Make Montserrat the new default font
          io.FontDefault = myMontserratFont;          
          console.log("Montserrat loaded successfully!");
      } else {
          console.error("Failed to load Montserrat.ttf. Check the path and file existence!");
      }
    }
    SetupUbuntuStyle();
  }
}

export class MinesApp {
  // HTML element that will contain the renderer's canvas
  private container: HTMLCanvasElement;

  // Three.js OrbitControls for camera manipulation
  private orbit: OrbitControls;

  // a single quad for drawing texture image
  private board: THREE.Mesh;
  private boardSize: number[];
  private boardMaterial: THREE.Material;

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
  public mouse_in_imgui:boolean;

  constructor(canvas: HTMLCanvasElement) {
    if(!ImGui){
      console.log("imgui lib not found\n");
    }

    this.container = canvas;
    ImGui_Impl.Init(canvas);
    this.clear_color = new ImGui.Vec4(0.9,0.8,0.7,1);
    this.renderer = new THREE.WebGLRenderer({ canvas: canvas, antialias: true });
    this.world = new World();

    this.pixelRatio = window.devicePixelRatio ? window.devicePixelRatio : 1.0;

    this.renderer.setPixelRatio(this.pixelRatio);
    this.onWindowResize();
    // this.container.appendChild(this.renderer.domElement);

    this.orbit = new OrbitControls(this.world.camera, this.renderer.domElement);
    this.orbit.minDistance = 0.1;
    this.orbit.maxDistance = 30;
    this.orbit.maxPolarAngle = 3.13;
    this.orbit.minPolarAngle = 0.01;
    this.orbit.target.set(0, 0, 0);
    this.orbit.update();

    this.ResizeBoard(9,9);

    this.bindEventListeners();
   
    this.mouse_global = new MouseCoord();   
    this.SetupKbdEvents();
    this.mouse_in_imgui = false;
    this.boardSize = [1,1]
    this.board = 
  }

  public ResizeBoard(rows:number, cols:number){
    if(this.board){

    }
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
    this.container.addEventListener("mousemove", (e)=>this.onMouseMove(e));
  }

  public onMouseMove(e) {
    const io = ImGui.GetIO();
    
    // Check if ImGui is capturing the mouse. If so, exit the handler.
    if (io.WantCaptureMouse) {
      this.mouse_in_imgui=true;
      this.orbit.enabled = false;
      return; 
    }
    this.mouse_in_imgui = false;
    this.orbit.enabled = true;

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

    this.renderImgui(time);
  }

  public renderImgui(time){
    ImGui_Impl.NewFrame(time);
    ImGui.NewFrame();
    
    if (ImGui.Begin("Menu")) {
      ImGui.Text("Yo yo yo Imgui js");
      
      // An interactive element
      if (ImGui.Button("Close Window")) {
        console.log("close");
      }

      // Check if the window is fully visible (or at least its title bar)
      // and if it's being dragged, you might want to wait until the drag is over,
      // but for a simple snap-back, you can clamp every frame.

      // --- Clamping Logic ---
      const window_pos = ImGui.GetWindowPos();
      const window_size = ImGui.GetWindowSize();
      const viewport_size = ImGui.GetMainViewport().Size;

      // Define the boundaries (e.g., minimum visible area on the top-left)
      // Let's ensure at least 50 pixels are visible on the edge.
      const padding = 5.0;

      const min_pos = new ImGui.ImVec2(padding , padding );
      const max_pos = new ImGui.ImVec2(viewport_size.x - window_size.x, viewport_size.y - window_size.y);

      // Clamp the position
      const new_pos = {...window_pos};
      if (new_pos.x < min_pos.x) new_pos.x = min_pos.x;
      if (new_pos.y < min_pos.y) new_pos.y = min_pos.y;
      if (new_pos.x > max_pos.x) new_pos.x = max_pos.x;
      if (new_pos.y > max_pos.y) new_pos.y = max_pos.y;

      // Only apply the position if it changed
      if (new_pos.x != window_pos.x || new_pos.y != window_pos.y)
      {
          // Use SetWindowPos to apply the clamped position
          // ImGuiCond_Always is often used here, but you might use ImGuiCond_Once 
          // or ImGuiCond_Appearing if you only want to set the initial position, 
          // or no condition if you want to force it every frame.
          ImGui.SetWindowPos(new_pos, ImGui.ImGuiCond.Always);
      }


      // End the window call
      ImGui.End();
    }

    // 4. Finalize the ImGui frame and render
    ImGui.EndFrame();

    // Rendering commands
    ImGui.SetNextWindowBgAlpha(1.0);
    ImGui.Render();

    ImGui_Impl.RenderDrawData(ImGui.GetDrawData());
  }

  public onWindowResize(): void {
    const width = window.innerWidth;
    const height = window.innerHeight;
    const aspect = width / height;
    this.world.camera.aspect = aspect;
    this.world.camera.updateProjectionMatrix();
    //@ts-ignore
    ImGui.IO.DisplaySize = new ImGui.Vec2(width, height);
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

function SetupUbuntuStyle() {
    const style = ImGui.GetStyle();
    const colors = style.Colors;

    // Helper function for quick color creation
    // ImGui uses x, y, z, w for R, G, B, A
    const color = (r: number, g: number, b: number, a: number = 1.0) => new ImGui.ImVec4(r, g, b, a);

    // --- Core Window & Title Bar Colors (Dark Charcoal) ---
    const charcoalBg = color(0.18, 0.18, 0.18, 1.00);
    const darkerCharcoal = color(0.15, 0.15, 0.15, 1.00);
    const activeCharcoal = color(0.60, 0.30, 0.20, 1.00);
    
    colors[ImGui.Col.WindowBg]         = charcoalBg;
    colors[ImGui.Col.ChildBg]          = darkerCharcoal;
    colors[ImGui.Col.TitleBg]          = darkerCharcoal;
    colors[ImGui.Col.TitleBgActive]    = activeCharcoal;
    colors[ImGui.Col.TitleBgCollapsed] = color(0.10, 0.10, 0.10, 1.00);
    
    // --- Text and Frame ---
    colors[ImGui.Col.Text]             = color(0.95, 0.95, 0.95, 1.00);
    colors[ImGui.Col.FrameBg]          = color(0.25, 0.25, 0.25, 1.00);
    colors[ImGui.Col.FrameBgHovered]   = color(0.30, 0.30, 0.30, 1.00);
    colors[ImGui.Col.FrameBgActive]    = color(0.35, 0.35, 0.35, 1.00);

    // --- Button (Orange/Red Accent) ---
    const ubuntuOrangeHover = color(0.85, 0.27, 0.07, 1.00);
    const ubuntuOrangeActive = color(0.65, 0.17, 0.05, 1.00);
    
    colors[ImGui.Col.Button]           = color(0.25, 0.25, 0.25, 1.00);
    colors[ImGui.Col.ButtonHovered]    = ubuntuOrangeHover;
    colors[ImGui.Col.ButtonActive]     = ubuntuOrangeActive;

    // --- Header / Selection (Blue Accent) ---
    const selectionBlueHover = color(0.26, 0.44, 0.53, 1.00);
    const selectionBlueActive = color(0.20, 0.34, 0.42, 1.00);
    
    colors[ImGui.Col.Header]           = color(0.25, 0.25, 0.25, 1.00);
    colors[ImGui.Col.HeaderHovered]    = selectionBlueHover;
    colors[ImGui.Col.HeaderActive]     = selectionBlueActive;

    // --- Sliders, Checkboxes, and Popups ---
    colors[ImGui.Col.SliderGrab]       = ubuntuOrangeHover;
    colors[ImGui.Col.SliderGrabActive] = ubuntuOrangeActive;
    colors[ImGui.Col.CheckMark]        = ubuntuOrangeHover;
    colors[ImGui.Col.PopupBg]          = color(0.20, 0.20, 0.20, 1.00);
    
    // --- General Style Tweaks (Ubuntu-like geometry) ---
    style.WindowRounding = 4.0; 
    style.FrameRounding = 3.0;
    style.GrabRounding = 3.0;
    style.WindowTitleAlign.x = 0.5; // Center title text
}