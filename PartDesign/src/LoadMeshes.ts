import * as THREE from 'three'
import { STLLoader } from "./STLLoader.js";
import {OBJLoader} from "./OBJLoader.js"

type GeometryCallback = (filename: string, geometry: THREE.BufferGeometry) => void;

function ReadSTL(filename, onLoadGeometry : GeometryCallback) {
  const reader = new FileReader();
  try {
    reader.onload = function () {
      const loader = new STLLoader(THREE.DefaultLoadingManager);
      const geometry = loader.parse(reader.result);
      onLoadGeometry(filename, geometry);
    };
    reader.readAsArrayBuffer(filename);
  } catch (err) {
    console.log(err.message);
  }
}

function ReadOBJ(filename, onLoadGeometry : GeometryCallback) {
  const reader = new FileReader();
  try {
    reader.onload = function () {
      var loader = new OBJLoader(THREE.DefaultLoadingManager);
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

function ParseMeshData(file, extension){
    if (extension === "stl"){
        ReadSTL(file, world);
    }else if(extension ==="obj"){
        ReadOBJ(file,world);
    }else{
        console.log(file , "unknown file type");
    }
}

export const LoadMeshes = (files, world: World) => {
    for (const file of files) {
        const extension = file.name.split('.').pop().toLowerCase();
        ParseMeshData(file, extension, world);
    }
}