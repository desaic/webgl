import * as THREE from "three";

import { OrbitControls } from "./OrbitControls.js";
import { TransformControls } from "./TransformControls.js";

import World from "./World.js";
import { Array3D } from "./Array3D.js";
import { GPUPicker } from "./gpupicker.js";
import { OBJExporter } from "./OBJExporter.js";
import { Vec3f } from "./Vec3f.js";
import { MakeDiamondCell, MakeGyroidCell,
  MakeFluoriteCell,MakeOctetCell
 } from "./Lattice.js";

//map density to thickness
import * as D2T from "./DensityToThickness.js";
let d2t = [];

var container, orbit;
var renderer, clock, world;
let control;
var gpuPicker;
var pixelRatio = 1.0;
const exporter = new OBJExporter();
const RENDER_FPS = 60;
const lensConf = {
  diameter: 60,
  z: 30,
  cellSize: 2,
  type: "diamond",
  thickness: 0.3,
  minThickness: 0.175,
};
let unitCelldx = 2 / 64;
const unitCell = new Array3D(65, 65, 65);
// +1 because sample values are defined on vertices instead of cell centers.
const NUM_SAMPLES = 201;
const thicknessArr = new Float32Array(NUM_SAMPLES);

function LookupThickness(rho){
  if(rho<=0){return 0;}
  if(rho>1){return lensConf.cellSize;}
  if(d2t.length == 0){
    d2t = SelectD2TArray(lensConf);
  }
  const thickness = LinearInterp(d2t, rho, D2T.DENSITY_STEP);
  return thickness;
}

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
  control.addEventListener("dragging-changed", function (event) {
    orbit.enabled = !event.value;
  });

  world.scene.add(control.getHelper());

  pixelRatio = window.devicePixelRatio ? 1.0 / window.devicePixelRatio : 1.0;
  gpuPicker = new GPUPicker(THREE, renderer, world.scene, world.camera);

  bindEventListeners();
  ComputeUnitCell(unitCell, lensConf);
  DrawSlice(world.quadTexture.image, lensConf);
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

const SaveSphere = async () => {
  const defaultName = "sp_d"+lensConf.diameter.toFixed(0) + ".obj";
  const objString = exporter.parse(world.unitSphere);
  DownloadTxtFile(defaultName, objString);
};

const SaveTxt = async () => {
  const intRad = Math.floor(lensConf.diameter).toString();
  const intCell = Math.floor(lensConf.cellSize).toString();
  const type = lensConf.type;
  const defaultName = type+"D"+intRad+"c"+intCell+".txt";
  const sampleDist = (lensConf.diameter / 2) / (NUM_SAMPLES - 1);
  let outString = "origin 0 0 0\n";
  outString += "voxelSize " + sampleDist.toString() + " " + sampleDist.toString() + " " + 
  sampleDist.toString() + "\n";
  outString +="grid\n";
  const size = thicknessArr.length;
  outString += size.toString() + " 1 1\n";  
  
  for (let x = 0; x < size; x++) {
    outString += thicknessArr[x].toFixed(4)+" ";
  }
  outString += "\n";
 DownloadTxtFile(defaultName, outString);

};

function UpdateSlice() {
  DrawSlice(world.quadTexture.image, lensConf);
  world.quadTexture.needsUpdate = true;
}

function UpdateThicknessUI(densityEle, thicknessEle){
  const thickness = LookupThickness(densityEle.value);
  thicknessEle.textContent = "thickness=" + thickness.toFixed(3) + " mm";
}

const diameterInput = document.getElementById("diameterInput") as HTMLInputElement
const cellSizeInput = document.getElementById("cellSizeInput") as HTMLInputElement
const minThickInput = document.getElementById("minThickInput") as HTMLInputElement

const bindEventListeners = () => {
  window.addEventListener("resize", onWindowResize, false);
  container.addEventListener("click", selectObj);

  const saveButton = document.getElementById("saveMesh");
  saveButton.addEventListener("click", SaveSphere);

  const saveTxtButton = document.getElementById("saveGridTxt");
  saveTxtButton.addEventListener("click", () => {
    FillThickness();
    SaveTxt();
  });

  diameterInput
    .addEventListener("change", function () {
      const diameter = Number(this.value);
      const r :number = diameter / 2;
      world.unitSphere.scale.set(r, r, r);
      world.unitSphere.updateMatrix();
      lensConf.diameter = diameter;
      UpdateSlice();
    });
  cellSizeInput
    .addEventListener("change", function () {
      lensConf.cellSize = Number(this.value);
      ComputeUnitCell(unitCell, lensConf);
      UpdateSlice();
    });
  minThickInput
    .addEventListener("change", function () {
      lensConf.minThickness = Number(this.value);
      UpdateSlice();
    });
  const zlayerSlider = document.getElementById("zlayer") as HTMLInputElement;
  const zlayerLabel = document.getElementById("zlayerLabel");
  zlayerSlider.addEventListener("change", function () {
    lensConf.z = (Number(zlayerSlider.value) * lensConf.diameter) / 1000;
    zlayerLabel.textContent = "z=" + lensConf.z + "mm";
    UpdateSlice();
  });
  const setZLayerButton = document.getElementById("setZLayerButton");
  setZLayerButton.addEventListener("click", () => {
    if (zlayerSlider) {
      zlayerSlider.value = '500';
      const event = new Event("change");
      zlayerSlider.dispatchEvent(event);
    }
  });

  const densityEle = document.getElementById("densityInput");
  const thicknessEle = document.getElementById("thicknessLabel");
  densityEle.addEventListener("change", (_event) => {
    UpdateThicknessUI(densityEle, thicknessEle);
  });
  const lookupButton = document.getElementById("lookupThickness");
  lookupButton.addEventListener("click", () => {
    UpdateThicknessUI(densityEle, thicknessEle);
  });

  
  const selectType = document.getElementById("latticeType") as HTMLSelectElement;
  selectType.addEventListener("change", (_event) => {
    lensConf.type = selectType.value;
    ComputeUnitCell(unitCell, lensConf);
    UpdateSlice();
    UpdateThicknessUI(densityEle, thicknessEle);
  });

};

const selectObj = (event) => {
  var inversePixelRatio = 1.0 / pixelRatio;
  var objId = gpuPicker.pick(
    event.clientX * inversePixelRatio,
    event.clientY * inversePixelRatio
  );
  if (objId < 0) {
    return null;
  }
  console.log("pick", objId);
  const selected = world.GetInstanceById(objId);
  if (selected) {
    control.attach(selected);
  }
};

function onWindowResize() {
  const aspect = window.innerWidth / window.innerHeight;
  world.camera.aspect = aspect;
  world.camera.updateProjectionMatrix();

  const ocam = world.quadCamera;
  ocam.left = -aspect;
  ocam.right = aspect;
  ocam.updateProjectionMatrix();

  renderer.setSize(window.innerWidth, window.innerHeight);
}

function SetPixel(x, y, image, color4b) {
  const w = image.width;
  const i0 = 4 * (x + y * w);
  image.data[i0] = color4b[0];
  image.data[i0 + 1] = color4b[1];
  image.data[i0 + 2] = color4b[2];
  image.data[i0 + 3] = color4b[3];
}

function ComputeUnitCell(grid, conf) {
  const size = grid.size;
  const dx = conf.cellSize / (size[0] - 1);
  unitCelldx = dx;
  const voxelSize = new Vec3f(dx, dx, dx);
  const cellSize = new Vec3f(conf.cellSize, conf.cellSize, conf.cellSize);
  if (conf.type == "diamond") {
    MakeDiamondCell(voxelSize, cellSize, grid);
  } else if(conf.type == "gyroid") {
    MakeGyroidCell(voxelSize, cellSize, grid);
  } else if(conf.type == "fluorite"){
    MakeFluoriteCell(voxelSize, cellSize, grid);
  }else if(conf.type == "octet"){
    MakeOctetCell(voxelSize, cellSize, grid);
  }
}

function LuneRho(R, r) {
  // index of solid material.
  const EpsI = 2.35;
  const r2 = r * r;
  let rho = (R * R - r2) * ((EpsI + 4) * R * R - 2 * r2);
  rho /= 3 * R * R * (EpsI - 1) * (2 * R * R - r2);
  return rho;
}

function SelectD2TArray(lensConf) {
  const cellSize = lensConf.cellSize;
  const tp = lensConf.type;
  return D2T.SelectArray(tp, cellSize);
}

function LinearInterp(arr, x, dx) {
  if (arr.length == 0) {
    return 0;
  }
  if (x <= 0) {
    return 0;
  }
  if (x < dx) {
    return (x / dx) * arr[1];
  }
  const i0 = Math.floor(x / dx);
  if (i0 >= arr.length - 1) {
    return arr[arr.length - 1];
  }
  const i1 = i0 + 1;
  const alpha = x / dx - i0;
  const val = (1 - alpha) * arr[i0] + alpha * arr[i1];
  return val;
}

function FillThickness() {
  const len = thicknessArr.length;
  const R = 1.0;
  d2t = SelectD2TArray(lensConf);
  for (let i = 0; i < len; i++) {
    const r = i / len;
    const rho = LuneRho(R, r);
    let t = LinearInterp(d2t, rho, D2T.DENSITY_STEP);
    if (t < lensConf.minThickness) {
      t = lensConf.minThickness;
    }
    thicknessArr[i] = t;
  }
}

const color4b = [100, 100, 100, 255];
const coord = new Vec3f();
function DrawSlice(image, conf) {
  FillThickness();
  const w = image.width;
  const h = image.height;
  const dx = conf.diameter / w;
  const dy = conf.diameter / h;
  const cellSize = conf.cellSize;
  const R = conf.diameter / 2;
  const z = conf.z;
  const unitSize = unitCell.size;
  const voxSize = cellSize / (unitSize[0] - 1);
  const unitz = z - cellSize * Math.floor(z / cellSize);
  const k = Math.floor(unitz / voxSize);
  const s = unitCell.size;
  for (let y = 0; y < h; y++) {
    const ymm = (y + 0.5 - h / 2) * dy;
    for (let x = 0; x < w; x++) {
      const xmm = (x + 0.5 - w / 2) * dx;
      const cellIndexX = Math.floor(xmm / cellSize);
      const cellIndexY = Math.floor(ymm / cellSize);
      const cellx = xmm - cellIndexX * cellSize;
      const celly = ymm - cellIndexY * cellSize;

      const i = Math.floor(cellx / voxSize);
      const j = Math.floor(celly / voxSize);
      let d = 1000;
      if (i < s[0] && j < s[1] && k < s[2]) {
        coord.x = cellx / unitCelldx;
        coord.y = celly / unitCelldx;
        coord.z = unitz / unitCelldx;
        d = unitCell.TrilinearInterp(coord, d);
      }
      let c = 0;
      const zmm = z - conf.diameter/2;
      const r = Math.sqrt(xmm * xmm + ymm * ymm + zmm * zmm);
      const ratio = r / R;
      let t = 0;
      const thicknessIdx = Math.floor(ratio * thicknessArr.length);
      if (thicknessIdx < thicknessArr.length) {
        t = thicknessArr[thicknessIdx];
      } else if (r < R) {
        t = lensConf.minThickness;
      }
      if (d < t) {
        c = 1;
      }
      color4b[0] = c * 250;
      color4b[1] = c * 150;
      color4b[2] = c * 50;
      SetPixel(x, y, image, color4b);
    }
  }
}

let renderElapsed = 0;

function Animate(_time) {
  var delta = clock.getDelta();
  renderElapsed += delta;
  if (renderElapsed > 0.9 / RENDER_FPS) {
    renderer.render(world.scene, world.camera);
    renderer.autoClear = false;
    renderer.render(world.quadScene, world.quadCamera);
    renderElapsed = 0;
  }
  requestAnimationFrame(Animate);
}

InitScene();
Animate(null);
