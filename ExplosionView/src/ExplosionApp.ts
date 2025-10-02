import * as THREE from "three";
import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { GPUPicker } from "./gpupicker.js";
import { LoadingMan } from "./LoadingMan.ts";
import { ExplosionNode, ExplosionGraph } from "./ExplosionGraph.ts";

export class ExplosionApp {
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

  private pixelRatio: number;

  private loadingMan: LoadingMan;

  public treeContainer: HTMLElement;

  public inputScale: number;
  public explosionGraph: ExplosionGraph;
  public selectedPartName: string;
  /**
   * @constructor
   * @description
   */
  constructor(canvas: HTMLElement) {
    this.container = canvas;
    this.inputScale = 1.0;
    this.explosionGraph = new ExplosionGraph();
    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.world = new World();
    this.gpuPicker = new GPUPicker(
      THREE,
      this.renderer,
      this.world.scene,
      this.world.camera
    );
    this.treeContainer = null;
    this.loadingMan = new LoadingMan();

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
    this.control.addEventListener("dragging-changed", (event) => {
      this.orbit.enabled = !event.value;
    });

    this.world.scene.add(this.control.getHelper());

    this.SetupLoadingManager();
    this.bindEventListeners();
    this.selectedPartName = "";
  }

  public SetupLoadingManager() {
    this.loadingMan.onLoad = (name, obj) => {
      console.log(name + " loaded");
      this.AddObject3D(name, obj);
    };
  }

  public ScaleMesh(mesh: THREE.Mesh, scale: number) {
    const positionAttribute = mesh.geometry.attributes.position;
    const positions = positionAttribute.array;
    for (let i = 0; i < positions.length; i++) {
      positions[i] *= scale;
    }
  }

  public AddObject3D(name: string, obj: THREE.Object3D) {
    if (obj instanceof THREE.Mesh) {
      console.log("adding mesh " + name);
      this.ScaleMesh(obj, this.inputScale);

      const partId = this.world.AddPart(obj);
      this.world.AddInstance(
        new THREE.Vector3(0, 0, 0),
        new THREE.Euler(0, 0, 0),
        partId
      );
    } else if (obj instanceof THREE.BufferGeometry) {
      console.log("adding a BufferedGeometry " + name);
      const mesh = new THREE.Mesh(obj, this.world.defaultMaterial);
      this.ScaleMesh(mesh, this.inputScale);

      const partId = this.world.AddPart(mesh);
      this.world.AddInstance(
        new THREE.Vector3(0, 0, 0),
        new THREE.Euler(0, 0, 0),
        partId
      );
    } else {
      console.log("unknown mesh object type " + name);
    }
  }

  public bindEventListeners() {
    window.addEventListener("resize", () => this.onWindowResize());
    this.container.addEventListener("click", (event) => this.selectObj(event));

    const fileInput = document.getElementById("file-input") as HTMLInputElement;
    fileInput.addEventListener("change", (event) => {
      const target = event.target as HTMLInputElement;
      if (target.files.length > 0) this.LoadMeshes(target.files);
      const e: HTMLInputElement = target;
      e.value = "";
    });

    const graphInput = document.getElementById(
      "graph-input"
    ) as HTMLInputElement;
    graphInput.addEventListener("change", (event) => {
      const target = event.target as HTMLInputElement;
      if (target.files.length > 0) this.LoadGraph(target.files[0]);
      const e: HTMLInputElement = target;
      e.value = "";
    });
  }
  /**
   * @method init
   * @description
   */
  public init(): void {}

  public async LoadGraph(file: File) {
    if (!file) {
      return;
    }
    console.log("loading graph " + file.name);
    const fileContent = await file.text();
    // 2. Split the content into individual lines
    const lines = fileContent.split(/\r?\n/);
    if (lines.length == 0) {
      return;
    }
    console.log(lines[0]);
    const g = new ExplosionGraph();
    const parentMap: Map<string, string> = new Map();
    for (let i = 0; i < lines.length; i++) {
      const tokens = lines[i].split(",").map((token) => token.trim());
      if (tokens.length < 2) {
        continue;
      }
      const name = tokens[0];
      const node = new ExplosionNode(name);
      node.disasOrder = parseInt(tokens[1]);
      node.direction.set(
        parseFloat(tokens[2]),
        parseFloat(tokens[3]),
        parseFloat(tokens[4])
      );
      const parentName = tokens[5];
      parentMap.set(name, parentName);
      g.addNode(node);
    }
    for (const [child, parent] of parentMap) {
      //special keyword for having no parent
      if (parent == "None") {
        continue;
      }
      const childNode = g.getNodeByName(child);
      const parentNode = g.getNodeByName(parent);
      if (!childNode || !parent) {
        //something's fcked up
        continue;
      }
      parentNode.addChild(childNode);
    }
    const oldGraph = this.explosionGraph;
    this.explosionGraph = g;
    g.needsUpdate = true;
    oldGraph.destroy();
  }

  public LoadMeshes(files: FileList) {
    if (!files) {
      return;
    }
    if (this.loadingMan.busy) {
      console.log("Error: file loading busy");
      return;
    }
    this.loadingMan.SetBusy(true);
    console.log(this.loadingMan.busy);
    this.loadingMan.LoadMeshes(files);
    console.log(this.loadingMan.busy);
  }

  /**
   * @method render
   * @description to be called in the main animation loop.
   */
  public render(): void {
    this.renderer.render(this.world.scene, this.world.camera);
    this.renderer.autoClear = false;
  }

  public toggleNode(name: string, event) {
    if (event && typeof event.stopPropagation === "function") {
      event.stopPropagation();
    }

    const node = this.explosionGraph.getNodeByName(name);
    if (node) {
      // If isCollapsed is undefined, treat it as false (expanded)
      node.isCollapsed = !node.isCollapsed;
      this.explosionGraph.needsUpdate = true;
    }
  }
  
  public updateTreeHTML(nodes: ExplosionNode[], depth = 0) {
    if (!nodes || nodes.length === 0) {
      return "";
    }
    let html = "";
    const paddingLeft = depth * 1.5; // Tailwind spacing unit (e.g., pl-6 for level 2)

    for (const node of nodes) {
      const expanded = !node.isCollapsed;
      const hasChildren = node.children && node.children.length > 0;

      // Determine class for visual state
      const nodeClass = hasChildren && !expanded ? "node-collapsed" : "";
      const icon = hasChildren ? (expanded ? "▼" : "►") : "•";
      const nodeTypeColor = hasChildren ? "text-indigo-600" : "text-gray-700";
      const nodeFont = hasChildren ? "font-medium" : "font-normal";

      html += `
            <div id="node-${node.name}"
                 class="tree-node ${nodeClass} ${nodeFont}"
                 style="padding-left: ${paddingLeft}rem;"
                 node-name="${node.name}"
                 onclick="window.app.toggleNode('${node.name}', event)"
            >
              <span class="inline-block w-4 mr-1 ${nodeTypeColor}" node-name="${node.name}">${icon}</span>
              <span class="text-sm ${nodeTypeColor}" node-name="${node.name}">${node.name}</span>
          `;

      if (hasChildren) {
        html += `<div class="children">`;
        html += this.updateTreeHTML(node.children, depth + 1);
        html += `</div>`;
      }

      html += `</div>`;
    }
    return html;
  }

  public onWindowResize(): void {
    const aspect = window.innerWidth / window.innerHeight;
    this.world.camera.aspect = aspect;
    this.world.camera.updateProjectionMatrix();

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
