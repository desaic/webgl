import * as THREE from './js/three.module.js';
import { OrbitControls } from './js/OrbitControls.js';
import {World} from './World.js';

const fps = 60;
let world;
let lastUpdateTime;
function Animate() {
    setTimeout(() => {
        requestAnimationFrame( Animate );
    }, 1000 / fps);
    const now = Date.now();
    const dt = now - lastUpdateTime;
    lastUpdateTime = now;
    if(world != undefined){        
        world.Step(dt/1000.0);
        world.Render();
    }
}

function InitScene()
{
    world = new World();
    window.addEventListener( 'resize', onWindowResize, false );
    lastUpdateTime = Date.now();
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