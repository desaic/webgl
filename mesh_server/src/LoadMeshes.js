import * as THREE from 'three'
import { GLTFLoader } from "./GLTFLoader.js";
import { STLLoader } from "./STLLoader.js";

import World from "./World.js";

function ReadSTL(buf, world){
	const loader = new STLLoader();
	const geometry = loader.parse(buf);
    world.geometries.push(geometry);
    const mesh = new THREE.Mesh(geometry, world.defaultMaterial);
    world.instances.push(mesh);
}

function Read3MF(buf, world){

}

function ParseMeshData(readerBuf, extension, world){
    if (extension === "stl"){
        ReadSTL(readerBuf, world);
    }else if(extension ==="3mf"){
        Read3MF(readerBuf, world);
    }else if(extension ==="obj"){

    }else if(extension ==="glb"){

    }else{
        console.log(file , "unknown file type");
    }
}

export const LoadMeshes = (files, world) => {
    for (const file of files) {
        const extension = file.name.split('.').pop().toLowerCase();
        const reader = new FileReader();
        try{
            reader.onload = function(){ParseMeshData(reader.result, extension, world);}
            reader.readAsArrayBuffer(file);
        }catch(err){
            console.log(err.message);
        }
        
    }
}