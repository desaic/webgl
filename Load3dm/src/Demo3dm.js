// Import libraries
import * as THREE from 'three'
import { OrbitControls } from './OrbitControls.js'
import rhino3dm from 'rhino3dm'
import { OBJExporter } from "./OBJExporter.js";
const file = './src/assets/bones.3dm'
let scene, camera, renderer

// wait until the rhino3dm library is loaded, then load the 3dm file
const rhino = await rhino3dm()
console.log('Loaded rhino3dm.')

let res = await fetch(file)
let buffer = await res.arrayBuffer()
let arr = new Uint8Array(buffer)
let doc = rhino.File3dm.fromByteArray(arr)

init()

let material = new THREE.MeshNormalMaterial()

let objects = doc.objects()
for (let i = 0; i < objects.count; i++) {
    let mesh = objects.get(i).geometry()
    if(mesh instanceof rhino.Mesh) {
        // convert all meshes in 3dm model into threejs objects
        let threeMesh = meshToThreejs(mesh, material)
        scene.add(threeMesh)
    }
}


function DownloadTxtFile(filename, blobString){
  const blob = new Blob([blobString], { type: "text/plain" });
  const url = URL.createObjectURL(blob);
  const downTmp = document.createElement("a");
  downTmp.href = url;
  downTmp.download = filename;
  document.body.appendChild(downTmp);
  downTmp.click();
  document.body.removeChild(downTmp);
  URL.revokeObjectURL(url);
}
const exporter = new OBJExporter();
const SaveMeshes = (meshes) => {  
  const filename = "meshes.obj";
  let totalString = "";
  let vertCount = 1;
  for (let i = 0; i < meshes.length; i++) {
    totalString += "o " + i.toString() + "\n";
    const verts = meshes[i].geometry.attributes.position.array;
    const indices = meshes[i].geometry.index.array;
    const numV = verts.length / 3;
    const numT = indices.length / 3;
    for (let vi = 0; vi < numV; vi ++) {
      totalString +=
        "v " +
        verts[3*vi] +
        " " +
        verts[3*vi + 1] +
        " " +
        verts[3*vi + 2] +
        "\n";
    }
    for (let ti = 0; ti < numT; ti++) {
      totalString +=
        "f " +
        (indices[3*ti] + vertCount) +
        " " +
        (indices[3*ti + 1] + vertCount) +
        " " +
        (indices[3*ti + 2] + vertCount) +
        "\n";
    }
    console.log(verts.length + " " + indices.length);
    vertCount += numV;
  }
  
  DownloadTxtFile(filename, totalString);
};

SaveMeshes(scene.children);

// BOILERPLATE //

function init(){

    THREE.Object3D.DEFAULT_UP = new THREE.Vector3(0,0,1)

    scene = new THREE.Scene()
    scene.background = new THREE.Color(1,1,1)
    camera = new THREE.PerspectiveCamera( 45, window.innerWidth/window.innerHeight, 1, 1000 )
    camera.position.set(26,-40,5)

    renderer = new THREE.WebGLRenderer({antialias: true})
    renderer.setPixelRatio( window.devicePixelRatio )
    renderer.setSize( window.innerWidth, window.innerHeight )
    document.body.appendChild( renderer.domElement )

    const controls = new OrbitControls( camera, renderer.domElement )

    window.addEventListener( 'resize', onWindowResize, false )
    animate()
}

function animate () {
    requestAnimationFrame( animate )
    renderer.render( scene, camera )
}

function onWindowResize() {
    camera.aspect = window.innerWidth / window.innerHeight
    camera.updateProjectionMatrix()
    renderer.setSize( window.innerWidth, window.innerHeight )
    animate()
}

function meshToThreejs(mesh, material) {
    const loader = new THREE.BufferGeometryLoader()
    const geometry = loader.parse(mesh.toThreejsJSON())
    return new THREE.Mesh(geometry, material)
}