import * as THREE from "three";

import {ExplosionApp} from "./ExplosionApp"

let app : ExplosionApp;

// Three.js Clock for managing time in animations
let clock: THREE.Clock;

const RENDER_FPS = 60;

declare global {
    // Note the capital "W"
    interface Window { app: ExplosionApp; }
}

function InitScene() {
  //hack

  const canvas = document.getElementById("myCanvas");
  const treeContainer = document.getElementById('treeContainer');
  app = new ExplosionApp(canvas);
  app.treeContainer = treeContainer;

  window.app = app;

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
  if (app.explosionGraph.needsUpdate) {
    if (app.treeContainer) {
      app.treeContainer.innerHTML = app.updateTreeHTML(
        app.explosionGraph.getRootNodes()
      );
      app.explosionGraph.needsUpdate = false;
    }
  }
  requestAnimationFrame(Animate);
}

let meshScaleDisplay = null
function updateMeshScale(sliderValueString) {
  const MESH_SCALES = [
    1e-3, // 0
    1e-2, // 1
    1e-1, // 2
    1, // 3 (initial value)
    1e1, // 4
    1e2, // 5
    1e3, // 6
  ];
  const index = parseInt(sliderValueString, 10);
  const selectedScale = MESH_SCALES[index];

  // Format the number for display (using scientific notation for very large/small numbers)
  let displayValue;
  if (index === 3) {
    // Display '1' cleanly
    displayValue = "1";
  } else if (index < 3) {
    // Display small numbers as 1e-X
    displayValue = selectedScale.toExponential(0);
  } else {
    // Display large numbers as 1eX
    displayValue = selectedScale.toExponential(0).replace("e+", "e");
  }
  if(meshScaleDisplay == null){
    meshScaleDisplay = document.getElementById('meshScaleDisplay');
  }
  meshScaleDisplay.textContent = displayValue;
  if(app){
    app.inputScale = selectedScale;
  }
  // Optional: You would typically trigger an event or function here to update the mesh rendering
  console.log(`Mesh Scale Updated: ${selectedScale}`);
}

const scaleSlider = document.getElementById('scaleSlider') as HTMLInputElement;
scaleSlider.oninput=()=>{updateMeshScale(scaleSlider.value);}
InitScene();
Animate(null);
updateMeshScale(scaleSlider.value);
