import * as THREE from "./three.module.js";

import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import {Array3D} from "./Array3D.js"
import { GPUPicker } from './gpupicker.js';
import { LoadMeshes } from './LoadMeshes.js';
import { OBJExporter } from './OBJExporter.js';
import {Vec3f } from './Vec3f.js'
import {DrawBeamDist, MakeDiamondCell} from './Lattice.js'

const {save} = window.__TAURI__.dialog
const {writeTextFile} = window.__TAURI__.fs

var container, orbit;
var renderer, mixer, clock, world;
let control;
var gpuPicker;
var pixelRatio = 1.0;
const exporter = new OBJExporter();

const lensConf = {
  diameter: 60,
  z: 30,
  cellSize: 2,
  type : 'diamond',
  thickness : 0.3
};

const unitCell = new Array3D(65,65,65);

function InitScene() {
  container = document.getElementById("myCanvas");
  world = new World();
  clock = new THREE.Clock();
  
  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio);
  onWindowResize();
  container.appendChild(renderer.domElement);

  orbit = new OrbitControls(world.camera, renderer.domElement);
  orbit.minDistance = 2;
  orbit.maxDistance = 1000;
  orbit.maxPolarAngle = 3.13;
  orbit.minPolarAngle = 0.01;
  orbit.target.set(0, 0, -0.2);
  orbit.update();
  
  control = new TransformControls(world.camera, renderer.domElement);
  control.addEventListener('dragging-changed', function (event) {
    orbit.enabled = !event.value;
  });

  world.scene.add(control.getHelper());

  pixelRatio = window.devicePixelRatio ? 1.0 / window.devicePixelRatio : 1.0;
  gpuPicker = new GPUPicker(THREE, renderer, world.scene, world.camera);
  
  bindEventListeners();
  ComputeUnitCell(unitCell, lensConf);
  DrawSlice(world.quadTexture.image, lensConf);
}

const BindFileInput = () => {
  const fileInput = document.getElementById("file-input");
  fileInput.addEventListener("change", (event) => {
    const file = event.target.files[0];
    if (!file) return;
    LoadMeshes(event.target.files, world);
  });
};

const SaveSphere = async ()=>{

  const filename = await save({
    filters: [{ name: "obj", extensions: ["obj"] }],
    defaultPath: 'sphere.obj',
  });

  if (filename == undefined) {
    return;
  }
  const objString = exporter.parse(world.unitSphere);

  try {
    if(filename){
        await writeTextFile(filename, objString);
    }
  } catch (err) {
    console.error('Error saving OBJ:', err);
  }
}

const bindEventListeners = () => {  
  window.addEventListener("resize", onWindowResize, false);
  container.addEventListener('click', selectObj)

  const saveButton = document.getElementById('saveMesh');
  saveButton.addEventListener('click', SaveSphere);

  document.getElementById('diameterInput').addEventListener('change', function() {
    const diameter = this.value;
    const r = diameter /2 ;
    world.unitSphere.scale.set(r,r,r);
    world.unitSphere.updateMatrix();
  });
}

const selectObj = (event) => {
  var inversePixelRatio = 1.0 / pixelRatio;
  var objId = gpuPicker.pick(event.clientX * inversePixelRatio, event.clientY * inversePixelRatio);
  if(objId<0){return null;}
  console.log("pick", objId);
  const selected = world.GetInstanceById(objId);
  if(selected){
    control.attach(selected);
  }
}

function onWindowResize() {
  const aspect = window.innerWidth / window.innerHeight;
  world.camera.aspect = aspect;
  world.camera.updateProjectionMatrix();
  
  const ocam = world.quadCamera;
  ocam.left=-aspect;
  ocam.right = aspect;
  ocam.updateProjectionMatrix();

  renderer.setSize(window.innerWidth, window.innerHeight);
}

function SetPixel(x,y,image,color4b){
  const w = image.width;
  const i0 = 4*(x+y*w);
  image.data[i0] = color4b[0];
  image.data[i0 + 1] = color4b[1];
  image.data[i0 + 2] = color4b[2];
  image.data[i0 + 3] = color4b[3];
}

function ComputeUnitCell(grid, conf){
  const size = grid.size;
  const dx = conf.cellSize / (size[0] - 1);
  const voxelSize = new Vec3f(dx, dx, dx);
  const cellSize = new Vec3f(conf.cellSize, conf.cellSize, conf.cellSize);
  MakeDiamondCell(voxelSize, cellSize, grid);
}

function DrawSlice(image, conf) {
  const w = image.width;
  const h = image.height;
  const dx = conf.diameter / w;
  const dy = conf.diameter / h;
  const cellSize = conf.cellSize;
  const R= conf.diameter / 2;
  const t=conf.thickness;
  const z = conf.z;
  const unitSize = unitCell.size;
  const voxSize = cellSize / (unitSize[0] - 1);
  const unitz = z - cellSize * Math.floor(z / cellSize);
  const k = Math.floor(unitz / voxSize);
  const s=unitCell.size;
  for (let y = 0; y < h; y++) {
    const ymm = (y + 0.5 - h/2 ) * dy;
    for (let x = 0; x < w; x++) {
      const xmm = (x + 0.5 - w / 2) * dx;
      const cellIndexX = Math.floor(xmm / cellSize);
      const cellIndexY = Math.floor(ymm / cellSize);
      const cellx = xmm - cellIndexX * cellSize;
      const celly = ymm - cellIndexY * cellSize;

      const i = Math.floor(cellx/voxSize);
      const j = Math.floor(celly / voxSize);
      let d = 1000;
      if(i< s[0] && j<s[1] && k<s[2]){
        d = unitCell.Get(i,j,k);
      }
      let c=0;
      if(d < t){
        c = 1;
      }
      const r = Math.sqrt(xmm * xmm + ymm*ymm);
      const ratio = r/R;
      const rx = cellx / cellSize;
      const ry = celly/cellSize;
      const color4b = [ratio * c * 250,ratio * c * 150,ratio * c * 50,255];
      SetPixel(x,y,image,color4b);
    }
  }
}

function Animate() {
  requestAnimationFrame(Animate);

  var delta = clock.getDelta();

  if (mixer) mixer.update(delta);

  renderer.render(world.scene, world.camera);  
  renderer.autoClear = false;
  const pr = renderer.getPixelRatio();
  renderer.setPixelRatio(1);
  renderer.render(world.quadScene, world.quadCamera);
  renderer.setPixelRatio(pr);
}

export { InitScene, Animate };
