import * as THREE from 'three'
import { STLLoader } from "./STLLoader.js";
import {OBJLoader} from "./OBJLoader.js"

function ReadSTL(filename, man: THREE.LoadingManager) {
  const reader = new FileReader();
  try {
    reader.onload = function () {
      const loader = new STLLoader(man);
      const geometry = loader.parse(reader.result);
    };
    reader.readAsArrayBuffer(filename);
  } catch (err) {
    console.log(err.message);
  }
}

function ReadOBJ(filename, man: THREE.LoadingManager) {
  const reader = new FileReader();
  try {
    reader.onload = function () {
      var loader = new OBJLoader(man);
      const object = loader.parse(reader.result);
      if(object.isGroup){
        for(const child of object.children){
            
        }
      }else{

      }
    };
    reader.readAsText(filename);
  } catch (err) {
    console.log(err.message);
  }
}

/// a single file may contain multiple meshes.
export const LoadMeshes = (file, man: THREE.LoadingManager) => {
  const extension = file.name.split(".").pop().toLowerCase();
  if (extension === "stl") {
    ReadSTL(file, man);
  } else if (extension === "obj") {
    ReadOBJ(file, man);
  } else {
    console.log(file, "unknown file type");
  }
};