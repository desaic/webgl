import {
  Box3,
  Vector3,
  Quaternion,
  Euler,
  Scene,
  BufferGeometry,
  BufferAttribute,
  Group,
  Mesh,
  MeshPhongMaterial,
  PerspectiveCamera,
  DirectionalLight,
  WebGLRenderer,
} from "three";
import { OrbitControls } from "./OrbitControls.js";
import { left_logo } from "./logo-left.js";
import { right_logo } from "./logo-right.js";

let canvas, renderer;

const scenes = [];

const yOffset = 32;
const fov = 75;
const aspect = 1;
const near = 1;
const far = 2000;

window.stepsList = [];
window.partList = [];
window.parts = [];
window.InitScene = InitScene;

const meshScale = 0.1;
const defaultMeshRot = new Euler(-Math.PI / 2, 0, -Math.PI / 2);
const ResetView = (scene) => {
  const orbit = scene.userData.controls;
  orbit.target.set(0, 0, 0);
  orbit.update();
};

const SetSceneCamera = (scene, camPos, fov) => {
  const camera = scene.userData.camera;
  camera.position.set(10 * camPos[0], 10 * camPos[1], 10 * camPos[2]);
  camera.fov = fov;
  camera.updateProjectionMatrix();
  ResetView(scene);
};

const MakeEmtpyScene = (element) => {
  const scene = new Scene();
  scene.userData.camera = new PerspectiveCamera(fov, aspect, near, far);
  const camera = scene.userData.camera;
  camera.up.set(0, 1, 0);
  scene.userData.element = element;
  const controls = new OrbitControls(
    scene.userData.camera,
    scene.userData.element
  );
  controls.minDistance = 2;
  controls.maxDistance = 100;
  controls.enablePan = true;
  controls.enableZoom = true;
  controls.maxPolarAngle = 3;
  scene.userData.controls = controls;

  const light = new DirectionalLight(0xffffff, 2);
  light.position.set(50, 50, 50);
  scene.add(light);

  const light2 = new DirectionalLight(0xffffff, 2);
  light2.position.set(5, 50, -50);
  scene.add(light2);

  const light3 = new DirectionalLight(0xffffff, 2);
  light3.position.set(0, -50, 0);
  scene.add(light3);

  const light4 = new DirectionalLight(0xffffff, 2);
  light4.position.set(-50, 0, 0);
  scene.add(light4);

  ResetView(scene, controls);
  return scene;
};

const InitGeometries = (parts) => {
  const geometries = [];
  for (let i = 0; i < parts.length; i++) {
    geometries[i] = new BufferGeometry();
    const farr = new Float32Array(parts[i].vertices).map((val) => 0.1 * val);
    geometries[i].setAttribute("position", new BufferAttribute(farr, 3));
    geometries[i].computeVertexNormals();
  }
  return geometries;
};

export function InitScene() {
  canvas = document.getElementById("c");

  const parts = window.parts;
  const geometries = InitGeometries(parts);

  // add parts here
  const partListEle = document.getElementById("part-list");
  const partList = window.partList;
  for (let i = 0; i < partList.length; i++) {
    // make a list item
    const element = document.createElement("div");
    element.className = "grid-item";
    const sceneElement = document.createElement("div");
    sceneElement.className = "mini-canvas";
    element.appendChild(sceneElement);
    partListEle.appendChild(element);

    // the element that represents the area we want to render the scene
    const scene = MakeEmtpyScene(sceneElement);
    for (let j = 0; j < partList[i].partIdxs.length; j++) {
      const partId = partList[i].partIdxs[j];
      const geometry = geometries[partId];
      const material = new MeshPhongMaterial({
        color: parts[partId].color,
      });
      const mesh = new Mesh(geometry, material);
      mesh.setRotationFromEuler(defaultMeshRot);
      scene.add(mesh);
    }
    SetSceneCamera(scene, partList[i].camera.position, partList[i].camera.fov);
    scenes.push(scene);
  }

  const stepsEle = document.getElementById("instruction-steps");
  const stepsList = window.stepsList;

  for (let i = 0; i < stepsList.length; i++) {
    // make a list item
    const element = document.createElement("div");
    element.className = "grid-item";
    const sceneElement = document.createElement("div");
    sceneElement.className = "mini-canvas";
    element.appendChild(sceneElement);

    const stepNumber = document.createElement("div");
    stepNumber.innerText = `${i + 1}`;
    stepNumber.className = "step-number";
    element.appendChild(stepNumber);
    stepsEle.appendChild(element);

    // the element that represents the area we want to render the scene
    const scene = MakeEmtpyScene(sceneElement);
    // overall rotation
    const group = new Group();
    group.setRotationFromEuler(defaultMeshRot);
    scene.add(group);
    for (let j = 0; j < stepsList[i].partIdxs.length; j++) {
      const partId = stepsList[i].partIdxs[j];
      const geometry = geometries[partId];
      const trans = parts[partId].transparent;
      const material = new MeshPhongMaterial({
        color: parts[partId].color,
      });
      if (trans) {
        material.transparent = true;
        material.opacity = 0.1;
        material.depthWrite = false;
      }
      const mesh = new Mesh(geometry, material);
      const pos = parts[partId].position;
      mesh.position.set(pos[0], pos[1], pos[2]);
      const q = new Quaternion();
      q.fromArray(parts[partId].rotation);
      mesh.setRotationFromQuaternion(q);
      group.add(mesh);
    }

    SetSceneCamera(
      scene,
      stepsList[i].camera.position,
      stepsList[i].camera.fov
    );
    if (group.children.length > 0) {
      group.updateMatrixWorld(true);
      const bbox = new Box3().setFromObject(group.children[0]);
      const boxCenter = new Vector3(0, 0, 0);
      boxCenter.copy(bbox.min);
      boxCenter.add(bbox.max);
      boxCenter.multiplyScalar(0.5);
      const orbit = scene.userData.controls;
      orbit.target.copy(boxCenter);
      orbit.target0.copy(boxCenter);
      const camera = scene.userData.camera;
      camera.position.add(boxCenter);
      orbit.update();
    }
    scenes.push(scene);
  }

  renderer = new WebGLRenderer({ canvas: canvas, antialias: true });
  renderer.setClearColor(0xffffff, 1);
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.setAnimationLoop(animate);

  bindEventListeners();
}

function updateSize() {
  const width = canvas.clientWidth;
  const height = canvas.clientHeight;

  if (canvas.width !== width || canvas.height !== height) {
    renderer.setSize(width, height, false);
  }
}

function animate() {
  updateSize();

  canvas.style.transform = `translateY(${window.scrollY}px)`;

  renderer.setScissorTest(false);
  renderer.clear();

  renderer.setScissorTest(true);

  scenes.forEach(function (scene) {
    // get the element that is a place holder for where we want to
    // draw the scene
    const element = scene.userData.element;

    // get its position relative to the page's viewport
    const rect = element.getBoundingClientRect();

    // check if it's offscreen. If so skip it
    if (
      rect.bottom < 0 ||
      rect.top > renderer.domElement.clientHeight ||
      rect.right < 0 ||
      rect.left > renderer.domElement.clientWidth
    ) {
      return; // it's off screen
    }

    // set the viewport
    const width = rect.right - rect.left;
    const height = rect.bottom - rect.top;
    const left = rect.left;
    const bottom = renderer.domElement.clientHeight - rect.bottom;

    renderer.setViewport(left, bottom, width, height);
    renderer.setScissor(left, bottom, width, height);

    const camera = scene.userData.camera;

    camera.aspect = width / height;
    camera.updateProjectionMatrix();
    scene.userData.controls.update();

    renderer.render(scene, camera);
  });
}

const HandleKeyboard = (event) => {
  switch (event.key) {
    case "Shift":
      break;
    case "v":
      break;
    case "Delete":
      break;
    case "Escape":
      break;
    case "d":
      break;
    default:
  }
};

const bindEventListeners = () => {
  window.addEventListener("keydown", HandleKeyboard);
  // window.addEventListener('scroll', function() {
  //   console.log('Scroll event detected');
  // });
};

const setPackResult = (text) => {
  const target = document.getElementById("pack-result");
  if (target) {
    target.innerText = text;
  }
};

const setPackDate = (text) => {
  const target = document.getElementById("packed-on-date");
  if (target) {
    target.innerText = text;
  }
};

const setContainerName = (text) => {
  const target = document.getElementById("container-name");
  if (target) {
    target.innerText = text;
  }
};

const setLeftLogo = (img_base64) => {
  const target = document.getElementById("logo-left");
  if (target) {
    const img = document.createElement("img");
    img.src = img_base64;
    img.style.maxHeight = "100%";
    img.style.maxWidth = "100%";

    target.appendChild(img);
  }
};

const setRightLogo = (img_base64) => {
  const target = document.getElementById("logo-right");
  if (target) {
    const img = document.createElement("img");
    img.src = img_base64;
    img.style.maxHeight = "100%";
    img.style.maxWidth = "100%";
    target.appendChild(img);
  }
};

const setTitle = (text) => {
  const target = document.getElementById("pdf-title");
  if (target) {
    target.innerText = text;
  }
};

window.SetContainerName = setContainerName;
window.SetLeftLogo = setLeftLogo;
window.SetRightLogo = setRightLogo;
window.SetPackResult = setPackResult;
window.SetPackDate = setPackDate;
window.SetTitle = setTitle;

window.SetLeftLogo(left_logo);
window.SetRightLogo(right_logo);
