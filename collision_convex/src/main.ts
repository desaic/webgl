import * as THREE from "three";

import {CollisionApp} from "./CollisionApp"

let app : CollisionApp;

// Three.js Clock for managing time in animations
let clock: THREE.Clock;

const RENDER_FPS = 60;

function InitScene() {
  const canvas = document.getElementById("myCanvas");
  app = new CollisionApp(canvas);
  clock = new THREE.Clock();
}

let renderElapsed = 0;

function Animate(_time) {
  var delta = clock.getDelta();
  renderElapsed += delta;
  if (renderElapsed > 0.9 / RENDER_FPS) {
    app.render();
    renderElapsed = 0;
  }
  requestAnimationFrame(Animate);
}

InitScene();
Animate(null);
