import * as THREE from "three";
import { World } from "./World.js"
import { OrbitControls } from "./OrbitControls.js";

let canvas, renderer;

const parts = [];
const scenes = [];

export function InitScene() {
  canvas = document.getElementById("c");

  const geometries = [
    new THREE.BoxGeometry(1, 1, 1),
    new THREE.SphereGeometry(0.5, 12, 8),
    new THREE.DodecahedronGeometry(0.5),
    new THREE.CylinderGeometry(0.5, 0.5, 1, 12),
  ];

  // add parts here
  const partList = document.getElementById("part-list");
  for (let i = 0; i < 6; i++) {
    const world = new World();
    const scene = world.scene;

    // make a list item
    const element = document.createElement("div");
    element.className = "grid-item";

    const sceneElement = document.createElement("div");
    sceneElement.className = "mini-canvas"
    element.appendChild(sceneElement);


    // the element that represents the area we want to render the scene
    scene.userData.element = sceneElement;
    partList.appendChild(element);


    const camera = world.camera;
    camera.position.z = 2;
    scene.userData.camera = camera;

    const controls = new OrbitControls(
      scene.userData.camera,
      scene.userData.element
    );
    controls.minDistance = 2;
    controls.maxDistance = 5;
    controls.enablePan = false;
    controls.enableZoom = false;
    scene.userData.controls = controls;

    // add one random mesh to each scene
    const geometry = geometries[(geometries.length * Math.random()) | 0];

    const material = new THREE.MeshStandardMaterial({
      color: new THREE.Color().setHSL(
        Math.random(),
        1,
        0.75,
        THREE.SRGBColorSpace
      ),
      roughness: 0.5,
      metalness: 0,
      flatShading: true,
    });

    scene.add(new THREE.Mesh(geometry, material));

    scene.add(new THREE.HemisphereLight(0xaaaaaa, 0x444444, 3));

    const light = new THREE.DirectionalLight(0xffffff, 1.5);
    light.position.set(1, 1, 1);
    scene.add(light);

    world.ResetView(controls);
    scenes.push(scene);
  }

  // add instructions here
  const instructionsList = document.getElementById("instruction-steps")
  for (let i = 0; i < 6; i++) {
    const world = new World();
    const scene = world.scene;

    // make a list item
    const element = document.createElement("div");
    element.className = "grid-item";


    const sceneElement = document.createElement("div");
    sceneElement.className = "mini-canvas"
    element.appendChild(sceneElement);

    const stepNumber = document.createElement("div");
    stepNumber.innerText = `${i+1}`
    stepNumber.className = "step-number"
    element.appendChild(stepNumber)

    // the element that represents the area we want to render the scene
    scene.userData.element = sceneElement;
    instructionsList.appendChild(element);


    const camera = world.camera;
    camera.position.z = 2;
    scene.userData.camera = camera;

    const controls = new OrbitControls(
      scene.userData.camera,
      scene.userData.element
    );
    controls.minDistance = 2;
    controls.maxDistance = 5;
    controls.enablePan = false;
    controls.enableZoom = false;
    scene.userData.controls = controls;

    // add one random mesh to each scene
    const geometry = geometries[(geometries.length * Math.random()) | 0];

    const material = new THREE.MeshStandardMaterial({
      color: new THREE.Color().setHSL(
        Math.random(),
        1,
        0.75,
        THREE.SRGBColorSpace
      ),
      roughness: 0.5,
      metalness: 0,
      flatShading: true,
    });

    scene.add(new THREE.Mesh(geometry, material));

    scene.add(new THREE.HemisphereLight(0xaaaaaa, 0x444444, 3));

    const light = new THREE.DirectionalLight(0xffffff, 1.5);
    light.position.set(1, 1, 1);
    scene.add(light);

    world.ResetView(controls);
    scenes.push(scene);
  }

  renderer = new THREE.WebGLRenderer({ canvas: canvas, antialias: true });
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

  renderer.setClearColor(0xffffff);
  renderer.setScissorTest(false);
  renderer.clear();

  renderer.setClearColor(0xe0e0e0);
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

    //camera.aspect = width / height; // not changing in this example
    //camera.updateProjectionMatrix();

    //scene.userData.controls.update();

    renderer.render(scene, camera);
  });
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