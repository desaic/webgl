import * as THREE from './js/three.module.js';
var scene ;
var camera ;
var renderer ;
var cube;
const fps = 1;
var frameCnt = 0;
function Animate() {
    setTimeout(() => {
        requestAnimationFrame( Animate );
    }, 1000 / fps);
    if(cube!=undefined){
        cube.rotation.x += 0.1;
        cube.rotation.y += 0.1;    

        // Get the first child node of an <ul> element
        var item = document.getElementById("text1");
        // Replace the first child node of <ul> with the newly created text node
        item.innerHTML = "FPS: " + frameCnt;
        frameCnt++;
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