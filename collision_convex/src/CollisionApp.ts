import { BufferGeometry, Euler, Mesh, MeshPhongMaterial, Object3D, Vector3, WebGLRenderer} from "three";
import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { GPUPicker } from "./gpupicker.js";
import { GUI, Controller } from 'lil-gui';
import { RoundedBoxGeometry } from "./RoundedBoxGeometry.js";

import { RoundedCuboid } from './RoundedCuboid'

export class CollisionApp {
  // HTML element that will contain the renderer's canvas
  private container: HTMLElement;

  private orbit: OrbitControls;

  // Three.js WebGL Renderer for rendering the 3D scene
  private renderer: WebGLRenderer;

  // meshes
  private world: World;

  private control: TransformControls;

  // GPU Picker for object selection (e.g., using a picking texture)
  private gpuPicker: any;

  // Flag to indicate whether a slice view should be shown
  private showSlice: boolean;

  private pixelRatio: number;

  private gui: GUI;
  private cuboidArr: RoundedCuboid[];
  public alpha: number;
  public scaleControl:Controller;
  /**
   * @constructor
   * @description
   */
  constructor(canvas: HTMLElement) {
    this.container = canvas;
    this.renderer = new WebGLRenderer({ antialias: true });
    this.world = new World();
    this.gpuPicker = new GPUPicker(
      this.renderer,
      this.world.scene,
      this.world.camera
    );
    this.alpha = 1.0;
    const cuboidA = new RoundedCuboid();
    const cuboidB = new RoundedCuboid();
    cuboidB.conf.x = 200;
    this.cuboidArr =[cuboidA, cuboidB];
    this.gui = new GUI();    
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

  public SetupGUI() {
    const folderA = this.gui.addFolder('Cuboic A');
    const confA = this.cuboidArr[0].conf;
    const confB = this.cuboidArr[1].conf;
    folderA.add(confA, 'x', -500, 500, 10);
    folderA.add(confA, 'y',  -500, 500, 10);
    folderA.add(confA, 'z',  -500, 500, 10);
    folderA.add(confA, 'rx', -4, 4, 0.1);
    folderA.add(confA, 'ry', -4, 4, 0.1);
    folderA.add(confA, 'rz', -4, 4, 0.1);
    const folderB = this.gui.addFolder('Cuboic B');
    folderB.add(confB, 'x',  -500, 500, 10);
    folderB.add(confB, 'y',  -500, 500, 10);
    folderB.add(confB, 'z',  -500, 500, 10);
    folderB.add(confB, 'rx', -4, 4, 0.1);
    folderB.add(confB, 'ry', -4, 4, 0.1);
    folderB.add(confB, 'rz', -4, 4, 0.1);

    const guiFun = {
      solveScale : ()=>{
        this.SolveScale();
        if(this.scaleControl){
          this.scaleControl.updateDisplay();
        }
      }
    };
    this.gui.add(guiFun, 'solveScale').name('Solve Scale');
    this.scaleControl = this.gui.add(this, 'alpha');  
  }

  public SolveScale(){
    this.alpha = 2;
  }

  public AddCuboidsToScene() {
    // A basic material doesn't react to light, so let's use a standard material for a better look.
    const defaultMaterial = new MeshPhongMaterial({ color: 0x0077ff });
    const numSegments = 2;
    const defaultPos = new Vector3(0,0,0);
    const defaultRot = new Euler(0,0,0);
    for (let i = 0; i < this.cuboidArr.length; i++) {            
      const w= this.cuboidArr[i].size.x;
      const h= this.cuboidArr[i].size.y;
      const d= this.cuboidArr[i].size.z;
      const rad = this.cuboidArr[i].sphereRadius;
      const geometry = new RoundedBoxGeometry(w, h, d, numSegments, rad);
      const cuboid = new Mesh(geometry, defaultMaterial);
      const partId = this.world.AddPart(cuboid);
      const instanceId = this.world.AddInstance(defaultPos, defaultRot, partId);
      this.cuboidArr[i].id = instanceId;
    }
    //lol
    const materialB = new MeshPhongMaterial({ color: 0xff3300 });
    const instance = this.world.GetInstanceById(this.cuboidArr[1].id) as Mesh;
    instance.material = materialB;
  }

  public UpdateCuboidsRender(){
    // copy ui config to cuboid state and rendering
    for(let i = 0;i<this.cuboidArr.length;i++){
      const cuboid = this.cuboidArr[i];
      cuboid.UpdateFromConf();
      const instance = this.world.GetInstanceById(cuboid.id);      
      instance.position.copy(cuboid.position);
      instance.rotation.set(cuboid.conf.rx, cuboid.conf.ry, cuboid.conf.rz);
      instance.scale.set(cuboid.alpha,cuboid.alpha,cuboid.alpha);

    }
  }

  public AddObject3D(name: string, obj:Object3D){
    if(obj instanceof Mesh){
      console.log("adding mesh" + name);
      const partId = this.world.AddPart(obj);
      this.world.AddInstance(
        new Vector3(0, 0, 0),
        new Euler(0, 0, 0),
        partId
      );
    }else if(obj instanceof BufferGeometry){
      console.log('adding a BufferedGeometry ' + name);
      const mesh = new Mesh(obj, this.world.defaultMaterial);
      const partId = this.world.AddPart(mesh);
      this.world.AddInstance(
        new Vector3(0, 0, 0),
        new Euler(0, 0, 0),
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
    this.UpdateCuboidsRender();
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
