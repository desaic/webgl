import * as THREE from "three";
import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { GPUPicker } from "./gpupicker.js";
import { LoadingMan } from "./LoadingMan.ts";
import { ExplosionNode, ExplosionGraph } from "./ExplosionGraph.ts";

const MESH_COLOR_PALETTE = [
    { x: 0.1, y: 0.45, z: 0.65, name: "Deep Ocean Blue" },
    { x: 0.9, y: 0.45, z: 0.1, name: "Sunset Orange" },
    { x: 0.4, y: 0.6, z: 0.35, name: "Muted Sage Green" },
    { x: 0.75, y: 0.65, z: 0.9, name: "Soft Lavender" },
    { x: 0.1, y: 0.85, z: 0.8, name: "Electric Cyan" },
    { x: 0.7, y: 0.2, z: 0.2, name: "Brick Red" },
    { x: 0.8, y: 0.75, z: 0.7, name: "Light Taupe" },
];

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
  public explosionScale: number;
  //map from a name to mesh instance
  public nameToMesh: Map<string, THREE.Mesh>;
  public edges:Map<string, THREE.Mesh>;
  public arcId:number;
  public arcMesh:THREE.Mesh;
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
    this.explosionScale = 1.0;
    this.nameToMesh = new Map<string, THREE.Mesh<any, any>>();

    this.arcMesh = this.MakeArc();
    this.arcId = this.world.AddPart(this.arcMesh)
    this.edges = new Map();
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
    let instance = null;
    //horrible hack to use the file name without suffix as mesh name.
    if (name && name.indexOf(".")>0) {
      name = name.substring(0, name.indexOf("."));
    }
    if (obj instanceof THREE.Mesh) {
      console.log("adding mesh " + name);
      this.ScaleMesh(obj, this.inputScale);

      const partId = this.world.AddPart(obj);
      instance = this.world.AddInstance(
        new THREE.Vector3(0, 0, 0),
        new THREE.Euler(0, 0, 0),
        partId
      );
      let m = obj as THREE.Mesh;
      if(m.geometry){
        m.geometry.computeBoundingBox();
      }
      this.nameToMesh.set(name, instance);
    } else if (obj instanceof THREE.BufferGeometry) {
      console.log("adding a BufferedGeometry " + name);
      const mesh = new THREE.Mesh(obj, this.world.defaultMaterial);
      this.ScaleMesh(mesh, this.inputScale);
      const partId = this.world.AddPart(mesh);
      instance = this.world.AddInstance(
        new THREE.Vector3(0, 0, 0),
        new THREE.Euler(0, 0, 0),
        partId
      );
    } else {
      console.log("unknown mesh object type " + name);
    }
    if (instance) {
      instance.name = name;
      const mat = instance.material;
      if(mat instanceof THREE.MeshPhongMaterial){
        const randColor = MESH_COLOR_PALETTE[instance.id % MESH_COLOR_PALETTE.length];
        mat.color.set(randColor.x, randColor.y,randColor.z);
      }
      this.nameToMesh.set(name, instance);
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

    const dxSlider = document.getElementById("dxSlider") as HTMLInputElement;
    const dxDisplay = document.getElementById("dxDisplay");

    dxSlider.addEventListener("input", () => {
      // Display the value directly, fixed to one decimal place
      dxDisplay.textContent = parseFloat(dxSlider.value).toFixed(1);
      this.explosionScale = Number(dxSlider.value);
    });
    // Set initial value
    dxDisplay.textContent = parseFloat(dxSlider.value).toFixed(1);
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
    //skip first line, assumed to be column names
    for (let i = 1; i < lines.length; i++) {
      const tokens = lines[i].split(",").map((token) => token.trim());
      if (tokens.length < 2) {
        continue;
      }
      const name = tokens[1];
      if(name.length == 0){
        // skip empty names
        continue;
      }
      const node = new ExplosionNode(name);
      node.disasOrder = 0;
      node.direction.set(
        parseFloat(tokens[2]),
        parseFloat(tokens[3]),
        parseFloat(tokens[4])
      );
      const parentName = tokens[0];
      parentMap.set(name, parentName);
      g.addNode(node);
    }
    for (const [child, parent] of parentMap) {
      //special keyword for having no parent
      if (parent == "None") {
        continue;
      }
      const childNode = g.getNodeByName(child);
      //only child nodes are added in the first pass
      let parentNode;
      if(!g.hasNode(parent)){
        parentNode = new ExplosionNode(parent);
        g.addNode(parentNode);
      }else{
         parentNode = g.getNodeByName(parent);
      }      
      if (!childNode || !parent) {
        //something's fcked up
        continue;
      }
      parentNode.addChild(childNode);
    }
    const oldGraph = this.explosionGraph;
    this.explosionGraph = g;
    g.needsUpdate = true;
    for (const [, v] of this.edges) {
      this.world.instances.remove(v);
    }
    this.edges.clear();
    for(let i = 0;i<g.nodes.length;i++){
      const instance = this.world.AddInstance(this.arcMesh.position, this.arcMesh.rotation, this.arcId);
      this.edges.set(g.nodes[i].name, instance);
    }

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

  public updateEdge(edge: THREE.Mesh, node :ExplosionNode, mesh:THREE.Mesh){
    const v0 = new THREE.Vector3(0,0,0);
    const v1 = new THREE.Vector3(1,1,1);
    const boxCenter = new THREE.Vector3(0, 0, 0);
    if(mesh){
      v1.copy(mesh.position);
      if (mesh.geometry.boundingBox) {
        const box = mesh.geometry.boundingBox;
        boxCenter.copy(box.max);
        boxCenter.add(box.min);
        boxCenter.multiplyScalar(0.5);
      }
      v1.add(boxCenter);
    }else{
      v1.copy(node.globalDisp);
    }

    const parentNode = node.parent;
    if (parentNode) {
      const m = this.nameToMesh.get(parentNode.name);
      const parent = m as THREE.Mesh;
      if (parent) {
        v0.copy(parent.position);
        const box = parent.geometry.boundingBox;
        boxCenter.set(0, 0, 0);
        boxCenter.copy(box.max);
        boxCenter.add(box.min);
        boxCenter.multiplyScalar(0.5);
        v0.add(boxCenter);
      }else{
        v0.copy(parentNode.globalDisp);
      }
    }
    const xAxis = new THREE.Vector3(v1.x, v1.y, v1.z);
    xAxis.sub(v0);
    let len = xAxis.lengthSq();
    if(len<1){
      xAxis.set(1,0,0);
    }else{
      xAxis.normalize();
    }
    len = Math.max(1.0, Math.sqrt(len));
    const yAxis = new THREE.Vector3(0,1,0);
    if(Math.abs(xAxis.y)>0.95){
      yAxis.set(0,0,1);
    }
    const zAxis = new THREE.Vector3(0,0,1);
    zAxis.crossVectors(xAxis, yAxis);
    zAxis.normalize();
    yAxis.crossVectors(zAxis, xAxis);
    const mat = new THREE.Matrix4(xAxis.x,yAxis.x, zAxis.x, 0,
      xAxis.y, yAxis.y, zAxis.y, 0,
      xAxis.z, yAxis.z, zAxis.z, 0,
      0,0,0,1
    );
    edge.quaternion.setFromRotationMatrix(mat);
    const scale = len;
    edge.scale.set(scale, scale, scale);
    edge.position.copy(v0);
    edge.matrixWorldNeedsUpdate;
    
  }

  public updateNodes(nodes: ExplosionNode[], visited: Set<string> = new Set()) {
    const localDisp = new THREE.Vector3();
    for (let i = 0;i<nodes.length;i++) {
      const node = nodes[i];
      if (!visited.has(node.name)) {
        visited.add(node.name);
      } else {
        continue;
      }
      const parent = node.parent;
      if(parent){
        //first copy parent's global displacement
        node.globalDisp.copy(parent.globalDisp);
        localDisp.copy(node.direction);
        localDisp.multiplyScalar(node.distance * this.explosionScale);
        node.globalDisp.add(localDisp);
      }else{
        //root don't move
        node.globalDisp.set(0,0,0);
      }

      const mesh = this.nameToMesh.get(node.name);
      if(mesh){
        mesh.position.copy(node.globalDisp);
        mesh.matrixWorldNeedsUpdate = true;
      }
      this.updateNodes(node.children, visited);
      const edge = this.edges.get(node.name);
      if(edge){
        this.updateEdge(edge, node, mesh);
      }
    }
  }

  public updateExplosion() {
    if (!this.explosionGraph || this.explosionGraph.empty()) {
      return;
    }
    this.updateNodes(this.explosionGraph.getRootNodes());
  }
  /**
   * @method render
   * @description to be called in the main animation loop.
   */
  public render(): void {
    this.updateExplosion();

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

  public MakeArc(){
    // Define two points for the arc, and a control point to lift it.
    const p1 = new THREE.Vector3(0, 0, 0);
    const p2 = new THREE.Vector3(1, 0, 0);
    // Control point for a nice upward curve
    const controlPoint = new THREE.Vector3(0.5, 0, 0.5);

    // Create a Quadratic Bezier Curve
    const curve = new THREE.QuadraticBezierCurve3(p1, controlPoint, p2);

    // Create geometry by sweeping a circle along the path (TubeGeometry)
    const tubeGeometry = new THREE.TubeGeometry(
        curve,       // path
        16,          // segments
        0.01,        // radius (thickness of the arc)
        4,           // radial segments
        false        // closed
    );

    // Main Material
    const arcMaterial = new THREE.MeshPhongMaterial({
        color: new THREE.Color(0.3,0.4,0.4),
        side: THREE.DoubleSide
    });
    const arcMesh = new THREE.Mesh(tubeGeometry, arcMaterial);
    return arcMesh;
  }

  public updateTreeHTML(
    nodes: ExplosionNode[],
    rendered: Set<string> = new Set(),
    depth = 0
  ) {
    if (!nodes || nodes.length === 0) {
      return "";
    }
    let html = "";
    const paddingLeft = depth * 1.5; // Tailwind spacing unit (e.g., pl-6 for level 2)

    for (const node of nodes) {
      if (rendered.has(node.name)) {
        continue;
      }
      rendered.add(node.name);
      const expanded = !node.isCollapsed;
      const hasChildren = node.children && node.children.length > 0;

      // Determine class for visual state
      const nodeClass = hasChildren && !expanded ? "node-collapsed" : "";
      const icon = hasChildren ? (expanded ? "▼" : "►") : "•";
      const nodeTypeColor = hasChildren ? "text-indigo-600" : "text-gray-700";
      const nodeFont = hasChildren ? "font-medium" : "font-normal";
      const isSelected = node === this.explosionGraph.selectedNode;
      const itemClasses = isSelected ? 'selected' : '';
      html += `
            <div id="node-${node.name}"
                 class="tree-node ${nodeClass} ${nodeFont}"
                 style="padding-left: ${paddingLeft}rem;"
                 node-name="${node.name}"
                 onclick="window.app.toggleNode('${node.name}', event)"
            >
              <span class="inline-block w-4 mr-1 ${nodeTypeColor}" node-name="${node.name}">${icon}</span>
              <span class="tree-node-item text-sm ${nodeTypeColor} ${itemClasses}" node-name="${node.name}">${node.name}</span>
          `;

      if (hasChildren) {
        html += `<div class="children">`;
        html += this.updateTreeHTML(node.children, rendered, depth + 1);
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
      const name = selected.name;
      const node = this.explosionGraph.getNodeByName(name);
      if(node){
        this.explosionGraph.selectedNode = node;
        this.explosionGraph.needsUpdate = true;
      }
    }
  }
}
