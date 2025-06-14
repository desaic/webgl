import * as THREE from "three";

import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { GPUPicker } from './gpupicker.js';
import { LoadMeshes } from './LoadMeshes.js';
var container, orbit;
var renderer, mixer, clock, world;
let control;
var gpuPicker;
var pixelRatio = 1.0;

var meshScale = 1.0;

export function InitScene() {
  container = document.getElementById("threejs-container");
  world = new World();
  clock = new THREE.Clock();
  
  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio);
  onWindowResize();
  container.appendChild(renderer.domElement);

  orbit = new OrbitControls(world.camera, renderer.domElement);
  orbit.minDistance = 1;
  orbit.maxDistance = 10000;
  orbit.target.set(0, 0, 0);
  orbit.update();
  
  control = new TransformControls(world.camera, renderer.domElement);
  control.addEventListener('dragging-changed', function (event) {
    orbit.enabled = !event.value;
  });

  world.scene.add(control.getHelper());

  pixelRatio = window.devicePixelRatio ? window.devicePixelRatio : 1.0;
  gpuPicker = new GPUPicker(THREE, renderer, world.scene, world.camera);
  
  bindEventListeners();

  BindFileInput();
}

const BindFileInput = () => {
  const fileInput = document.getElementById("file-input");
  fileInput.addEventListener("change", (event) => {
    const file = event.target.files[0];
    if (!file) return;
    LoadMeshes(event.target.files, world);
  });
};

const bindEventListeners = () => {  
  window.addEventListener("resize", onWindowResize, false);
  container.addEventListener('click', selectObj);
    
  const meshscaleSlider= document.getElementById("meshScale");
  meshscaleSlider.addEventListener("input", (event) => {
    document.getElementById('meshScaleVal').innerText = meshscaleSlider.value;
    meshScale = 10**parseFloat(meshscaleSlider.value);
    SetMeshScales(world, meshScale);
  });
  // Download button functionality
//  document.getElementById('downloadButton').addEventListener('click', () => {
//    downloadJSON(world.scene.toJSON(), 'scene-data.json');
//  });

}

const selectObj = (event) => {
  var objId = gpuPicker.pick(event.clientX * pixelRatio, event.clientY * pixelRatio);
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

// Function to download JSON
function downloadJSON(json, filename = 'data.json') {
  const jsonString = JSON.stringify(json, null, 2);
  const blob = new Blob([jsonString], { type: 'application/json' });
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = filename;
  document.body.appendChild(link);
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(link.href);
}

function SetMeshScales(world, scale){
    world.instances.scale.set(scale, scale, scale);
    world.instances.updateMatrix();
  
}

export function Animate() {
  requestAnimationFrame(Animate);

  var delta = clock.getDelta();

  if (mixer) mixer.update(delta);

  renderer.render(world.scene, world.camera);
}

window.InitScene = InitScene;
window.Animate = Animate;
