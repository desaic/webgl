import * as THREE from './js/three.module.js';
import { OrbitControls } from './js/OrbitControls.js';

var scene ;
var camera ;
let camControl;
var renderer ;
var cube;
const fps = 60;
var frameCnt = 0;
function Animate() {
    setTimeout(() => {
        requestAnimationFrame( Animate );
    }, 1000 / fps);
    if(cube!=undefined){ 

        frameCnt++;
        camControl.update();
    }
    if(scene != undefined){
        renderer.render( scene, camera );
    }
}

function InitScene()
{
    scene = new THREE.Scene();
    const camZnear = 0.1;
    const camZfar = 1000;
    camera = new THREE.PerspectiveCamera( 75, window.innerWidth / window.innerHeight, camZnear, camZfar );
    renderer = new THREE.WebGLRenderer();
    renderer.setSize( window.innerWidth, window.innerHeight );
    document.body.appendChild( renderer.domElement );
    camControl = new OrbitControls(camera, renderer.domElement);
    camControl.minDistance = camZnear;
    camControl.maxDistance = camZfar;
    camControl.maxPolarAngle = Math.PI / 2;
    camControl.enableDamping = true; // an animation loop is required when either damping or auto-rotation are enabled
    camControl.dampingFactor = 0.02;
    camControl.rotateSpeed = 30;

    camControl.screenSpacePanning = false;

    var geometry = new THREE.BoxGeometry( 1, 1, 1 );
    var material = new THREE.MeshBasicMaterial( { color: 0x00ff00 } );
    cube = new THREE.Mesh( geometry, material );
    scene.add( cube );
    
    camera.position.z = 5;  
    console.log(cube);
    window.addEventListener( 'resize', onWindowResize, false );
    Animate();    
}

function onWindowResize() {

  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();

  renderer.setSize( window.innerWidth, window.innerHeight );

}

export {InitScene, Animate};