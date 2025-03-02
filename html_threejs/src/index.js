import * as THREE from "three";

import { OrbitControls } from "./OrbitControls.js";

import World from "./World.js";
var container, orbit;
var renderer, world;
var pixelRatio = 1.0;
var canvas;
//disable picking when dragging.
var dragging = false;
export function InitScene() {
  container = document.getElementById("threejs-container");
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
  
  pixelRatio = window.devicePixelRatio ? 1.0 / window.devicePixelRatio : 1.0;
  canvas = renderer.domElement;
  
  bindEventListeners();
  world.ResetView(orbit);
}

const HandleKeyboard = (event)=>{
  switch (event.key) {
    case 'Shift':
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
  window.addEventListener('keydown', HandleKeyboard);
}

function onWindowResize() {
  world.camera.aspect = window.innerWidth / window.innerHeight;
  world.camera.updateProjectionMatrix();

  renderer.setSize(window.innerWidth, window.innerHeight);
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
