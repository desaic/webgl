import * as THREE from 'three'
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
            world.instances.add(mesh);
		}
		reader.readAsArrayBuffer(filename)
	}catch(err){
		console.log(err.message);
	}
}

function Sort3MFInstances(g, world, mat4){
  const myMat=new THREE.Matrix4();
  myMat.copy(mat4);
  myMat.multiply(g.matrix);
  if(g.isMesh){
    const gid = g.geometry.id;
    if(world.GetGeometryById(gid) == null){
      world.geometries.push(g.geometry);
    }    
    g.position.setFromMatrixPosition(myMat);
    g.rotation.setFromRotationMatrix(myMat);
    world.instances.add(g);
  }
  for(var i = 0;i<g.children.length;i++){
    Sort3MFInstances(g.children[i], world, myMat);
  }
}

function Read3MF(filename, world) {
  const reader = new FileReader();
  try {
    reader.onload = function () {
      const loader = new ThreeMFLoader();
      const group = loader.parse(reader.result);
      const m = new THREE.Matrix4();
      m.identity();
      Sort3MFInstances(group, world, m);
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
            world.instances.add(child);
        }
      }else{world.instances.add(object);}
      
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