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

export function InitScene() {
  container = document.getElementById("threejs-container");
  world = new World();
  clock = new THREE.Clock();
  
  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio);
  onWindowResize();
  container.appendChild(renderer.domElement);

  orbit = new OrbitControls(world.camera, renderer.domElement);
  orbit.minDistance = 2;
  orbit.maxDistance = 1000;
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
  container.addEventListener('click', selectObj)
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

export function Animate() {
  requestAnimationFrame(Animate);

  var delta = clock.getDelta();

  if (mixer) mixer.update(delta);

  renderer.render(world.scene, world.camera);
}

window.InitScene = InitScene;
window.Animate = Animate;
