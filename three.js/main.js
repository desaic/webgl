import * as THREE from './js/three.module.js';
import { OrbitControls } from './js/OrbitControls.js';
import {World} from './World.js';

const fps = 60;
let world;
function Animate() {
    setTimeout(() => {
        requestAnimationFrame( Animate );
    }, 1000 / fps);
    if(world != undefined){
        world.render();
    }
}

function InitScene()
{
    world = new World();
    window.addEventListener( 'resize', onWindowResize, false );
    Animate();    
}

function onWindowResize() {
  if(world != undefined){
    world.camera.aspect = window.innerWidth / window.innerHeight;
    world.camera.updateProjectionMatrix();
    world.renderer.setSize( window.innerWidth, window.innerHeight );  
  }
}

export {InitScene, Animate};