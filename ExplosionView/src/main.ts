import * as THREE from "three";

import {ExplosionApp} from "./ExplosionApp"

let app : ExplosionApp;

// Three.js Clock for managing time in animations
let clock: THREE.Clock;

const RENDER_FPS = 60;

function InitScene() {
  const canvas = document.getElementById("myCanvas");
  const treeContainer = document.getElementById('treeContainer');
  app = new ExplosionApp(canvas);
  app.treeContainer = treeContainer;
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
