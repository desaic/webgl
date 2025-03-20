import * as THREE from "./three.module.js";

import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { GPUPicker } from './gpupicker.js';
import { LoadMeshes } from './LoadMeshes.js';
import { OBJExporter } from './OBJExporter.js';

const {save} = window.__TAURI__.dialog
const {writeTextFile} = window.__TAURI__.fs

var container, orbit;
var renderer, mixer, clock, world;
let control;
var gpuPicker;
var pixelRatio = 1.0;
const exporter = new OBJExporter();

const uiConf = {
  diameter: 60,
  z: 30,
  cellSize: 2,
};

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

  DrawSlice(world.quadTexture.image, uiConf);
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

function DrawSlice(image, conf) {
  const w = image.width;
  const h = image.height;
  console.log(w + " x " + h);
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      let dx = (w/2-x);
      let dy = (h/2-y);
      dx = 4*dx*dx/w/w;
      dy = 4*dy*dy/h/h;
      const dist = Math.sqrt(dx + dy);
      const color4b = [dist * 250, dist * 100, dist * 50,255];
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
