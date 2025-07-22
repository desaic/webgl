import { STLLoader } from "./STLLoader.js"
import {OBJLoader} from "./OBJLoader.ts"
import {LoadingMan} from "./LoadingMan.ts"

function ReadSTL(file: File, man: LoadingMan) {
  const reader = new FileReader();
  try {
    reader.onload = function () {
      const loader = new STLLoader();
      const geometry = loader.parse(reader.result);
      man.onLoad(file.name, geometry);
    };
    reader.readAsArrayBuffer(file);
  } catch (err) {
    console.log(err.message);
  }
}

function ReadOBJ(file: File, man: LoadingMan) {
  const reader = new FileReader();
  try {
    reader.onload = function () {
      var loader = new OBJLoader(man, file.name);
      const object = loader.parse(reader.result);
      if (object.isGroup) {
        for (const child of object.children) {
          man.onLoad(child.name, child);
        }
      } else {
      }
    };
    reader.readAsText(file);
  } catch (err) {
    console.log(err.message);
  }
}

/// a single file may contain multiple meshes.
export const LoadMeshes = (file: File, man: LoadingMan) => {
  const extension = file.name.split(".").pop().toLowerCase();
  if (extension === "stl") {
    ReadSTL(file, man);
  } else if (extension === "obj") {
    ReadOBJ(file, man);
  } else {
    console.log(file, "unknown file type");
  }
};