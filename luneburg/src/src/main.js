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
  world.camera.aspect = window.innerWidth / window.innerHeight;
  world.camera.updateProjectionMatrix();

  renderer.setSize(window.innerWidth, window.innerHeight);
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
