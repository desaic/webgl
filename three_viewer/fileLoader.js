import * as THREE from "three";
import { OBJLoader } from "three/examples/jsm/loaders/OBJLoader.js";
import { STLLoader } from "three/examples/jsm/loaders/STLLoader.js";

const SUPPORTED_EXTENSIONS = [".obj", ".stl"];

function loadOBJ(text, material) {
  const object = new OBJLoader().parse(text);
  object.traverse((child) => {
    if (child.isMesh) child.material = material;
  });
  return object;
}

function loadSTL(buffer, material) {
  const geometry = new STLLoader().parse(buffer);
  geometry.computeVertexNormals();
  return new THREE.Mesh(geometry, material);
}

function parseFile(file, defaultMaterial) {
  return new Promise((resolve, reject) => {
    const name = file.name.toLowerCase();
    const ext = SUPPORTED_EXTENSIONS.find((e) => name.endsWith(e));
    if (!ext) {
      reject(
        new Error(`Unsupported file. Use ${SUPPORTED_EXTENSIONS.join(" or ")}`)
      );
      return;
    }

    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const object =
          ext === ".obj"
            ? loadOBJ(e.target.result, defaultMaterial)
            : loadSTL(e.target.result, defaultMaterial);
        resolve(object);
      } catch (err) {
        reject(err);
      }
    };
    reader.onerror = () => reject(new Error("Could not read file"));

    // STL can be binary or ASCII -> ArrayBuffer works for both;
    // OBJLoader.parse expects a string, so read as text for .obj.
    if (ext === ".obj") {
      reader.readAsText(file);
    } else {
      reader.readAsArrayBuffer(file);
    }
  });
}

export function setupFileHandling(app, { loadBtn, fileInput, statusEl }) {
  loadBtn.addEventListener("click", () => fileInput.click());

  fileInput.addEventListener("change", () => {
    const file = fileInput.files && fileInput.files[0];
    fileInput.value = ""; // allow re-loading the same file
    if (!file) return;

    statusEl.textContent = `Loading ${file.name}...`;
    parseFile(file, app.defaultMaterial)
      .then((object) => {
        app.setModel(object);
        statusEl.textContent = `Loaded ${file.name}`;
      })
      .catch((err) => {
        console.error(err);
        statusEl.textContent = `Failed: ${err.message}`;
      });
  });
}
