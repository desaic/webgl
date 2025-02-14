import * as THREE from "three";

import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { GPUPicker } from './gpupicker.js';
import { LoadMeshes } from './LoadMeshes.js';
var container, orbit;
var renderer, world;
let control;
var gpuPicker;
var pixelRatio = 1.0;
var infoElement;
var canvas;
//disable picking when dragging.
var dragging = false;
export function InitScene() {
  container = document.getElementById("threejs-container");
  infoElement = document.getElementById('info');
  world = new World();

  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setPixelRatio(window.devicePixelRatio);
  onWindowResize();
  container.appendChild(renderer.domElement);

  orbit = new OrbitControls(world.camera, renderer.domElement);
  orbit.minDistance = 2;
  orbit.maxDistance = 1000;
  orbit.target.set(0, 0, 0);
  orbit.update();
  
  control = new TransformControls(world.camera, renderer.domElement);
  control.addEventListener('dragging-changed', function (event) {
    orbit.enabled = !event.value;
    //cleared by click events.
    if(!dragging){ dragging = event.value;}
  });
  
  world.scene.add(control.getHelper());

  pixelRatio = window.devicePixelRatio ? 1.0 / window.devicePixelRatio : 1.0;
  gpuPicker = new GPUPicker(THREE, renderer, world.scene, world.camera);
  canvas = renderer.domElement;
  
  bindEventListeners();

  BindFileInput();
  world.ResetView(orbit);
}

const BindFileInput = () => {
  const fileInput = document.getElementById("file-input");
  fileInput.addEventListener("change", (event) => {
    const file = event.target.files[0];
    if (!file) return;
    LoadMeshes(event.target.files, world);
  });
};

const HandleKeyboard = (event)=>{
  switch (event.key) {
    case 'Shift':
      break;
    case 't':
      control.setMode("translate");
      break;
    case 'r':
      control.setMode("rotate");
      break;
    case 's':
      break;
    case 'v':
      world.ResetView(orbit);
      break;
    case 'Delete':
      break;
    case 'Escape':
      break;
    case 'd':
      break;
    default:
  }
}
const bindEventListeners = () => {  
  window.addEventListener("resize", onWindowResize, false);
  container.addEventListener('click', selectObj);
  control.addEventListener("change", () => {
    ShowSelectedInfo(world.selectedInstance);
  });
  // Download button functionality
  // document.getElementById('downloadButton').addEventListener('click', () => {
  //   downloadJSON(world.scene.toJSON(), 'scene-data.json');
  // });
  
  window.addEventListener('keydown', HandleKeyboard);
}

const ShowSelectedInfo = (selection) => {
  if(selection.length>0){
    const s = selection[0];
    const { x, y, z } = s.position;
    const { _x: rx, _y: ry, _z: rz } = s.rotation;
    infoElement.textContent = `
    Pos: (${x.toFixed(2)}, ${y.toFixed(2)}, ${z.toFixed(2)})
    Rot: (${rx.toFixed(2)}, ${ry.toFixed(2)}, ${rz.toFixed(2)})
    `;
  }
}

const selectObj = (event) => {
  if (dragging) {
    //was dragging transform control
    dragging = false;
    return;
  }
  var inversePixelRatio = 1.0 / pixelRatio;
  var objId = gpuPicker.pick(event.clientX * inversePixelRatio, event.clientY * inversePixelRatio);
  if(objId<0){return null;}
  console.log("pick", objId);
  const selected = world.GetInstanceById(objId);
  if(selected){
    control.attach(selected);
    world.selectedInstance = [selected];
    ShowSelectedInfo(world.selectedInstance);
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

function Render() {
  renderer.render(world.scene, world.camera);
}

export function Animate() {
  requestAnimationFrame(Animate);
  Render();
}

window.InitScene = InitScene;
window.Animate = Animate;
