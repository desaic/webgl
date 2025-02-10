import * as THREE from 'three'
import { GLTFLoader } from "./GLTFLoader.js";
import { STLLoader } from "./STLLoader.js";
import {OBJLoader} from "./OBJLoader.js"
import World from "./World.js";
import {ThreeMFLoader} from "./3MFLoader.js";

function ReadSTL(filename, world){
    const reader = new FileReader();
	try{
		reader.onload = function () {
            const loader = new STLLoader();
            const geometry = loader.parse(reader.result);			
            world.geometries.push(geometry);
            const mesh = new THREE.Mesh(geometry, world.defaultMaterial);
            world.instances.push(mesh);
		}
		reader.readAsArrayBuffer(filename)
	}catch(err){
		console.log(err.message);
	}
}

function Read3MF(filename, world) {
  const reader = new FileReader();
  try {
    reader.onload = function () {
      const loader = new ThreeMFLoader();
      const group = loader.parse(reader.result);
      world.instances.push(group);
    };
    reader.readAsArrayBuffer(filename);
  } catch (err) {
    console.log(err.message);
  }
}

function ReadOBJ(filename, world) {
  const reader = new FileReader();
  try {
    reader.onload = function () {
      var loader = new OBJLoader();
      const object = loader.parse(reader.result);
      if(object.isGroup){
        for(const child of object.children){
            world.instances.push(child);
        }
      }else{world.instances.push(object);}
      
    };
    reader.readAsText(filename);
  } catch (err) {
    console.log(err.message);
  }
}

function ParseMeshData(file, extension, world){
    if (extension === "stl"){
        ReadSTL(file, world);
    }else if(extension ==="3mf"){
        Read3MF(file, world);
    }else if(extension ==="obj"){
        ReadOBJ(file,world);
    }else if(extension ==="glb"){

    }else{
        console.log(file , "unknown file type");
    }
}

export const LoadMeshes = (files, world) => {
    for (const file of files) {
        const extension = file.name.split('.').pop().toLowerCase();
        ParseMeshData(file, extension, world);
    }
}