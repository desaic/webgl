import * as THREE from './js/three.module.js';
var scene ;
var camera ;
var renderer ;
var cube;

function Animate() {
    requestAnimationFrame( Animate );
    if(cube!=undefined){
        cube.rotation.x += 0.1;
        cube.rotation.y += 0.1;    
    }
    if(scene != undefined){
        renderer.render( scene, camera );
    }
}

function InitScene()
{
    scene = new THREE.Scene();
    camera = new THREE.PerspectiveCamera( 75, window.innerWidth / window.innerHeight, 0.1, 1000 );
    renderer = new THREE.WebGLRenderer();
    renderer.setSize( window.innerWidth, window.innerHeight );
    document.body.appendChild( renderer.domElement );
    
    var geometry = new THREE.BoxGeometry( 1, 1, 1 );
    var material = new THREE.MeshBasicMaterial( { color: 0x00ff00 } );
    cube = new THREE.Mesh( geometry, material );
    scene.add( cube );
    
    camera.position.z = 5;  
    console.log(cube);
    Animate();
    
}

export {InitScene, Animate};