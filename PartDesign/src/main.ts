import * as THREE from "three";

import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { Array3D } from "./Array3D.js";
import { GPUPicker } from "./gpupicker.js";
import { OBJExporter } from "./OBJExporter.js";
import { Vec3f } from "./Vec3f.js";
import {PartDesignApp} from "./PartDesignApp"

let app : PartDesignApp;

// Three.js Clock for managing time in animations
let clock: THREE.Clock;

const RENDER_FPS = 60;

function InitScene() {
  const canvas = document.getElementById("myCanvas");
  app = new PartDesignApp(canvas);
  clock = new THREE.Clock();
}

function DownloadTxtFile(filename:string, blobString:string){
  const blob = new Blob([blobString], { type: "text/plain" });
  const url = URL.createObjectURL(blob);
  const downTmp = document.createElement("a") as HTMLAnchorElement;
  downTmp.href = url;
  downTmp.download = filename;
  document.body.appendChild(downTmp);
  downTmp.click();
  document.body.removeChild(downTmp);
  URL.revokeObjectURL(url);
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
