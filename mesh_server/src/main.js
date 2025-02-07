import * as THREE from './three.module.js';

import { OrbitControls } from './OrbitControls.js';
import { GLTFLoader } from './GLTFLoader.js';

var container, controls;
var camera, scene, renderer, mixer, clock;

function InitScene() {

  container = document.createElement( 'div' );
  document.body.appendChild( container );

  camera = new THREE.PerspectiveCamera( 45, window.innerWidth / window.innerHeight, 1, 100 );

  scene = new THREE.Scene();
  
  clock = new THREE.Clock();

  const ambientLight = new THREE.AmbientLight( 0xffffff, 0.1 );
  scene.add( ambientLight );
  const col = new THREE.Color(0xeeeeee);
  const pointLight = new THREE.PointLight( 0xffffff, 10,100 );
  pointLight.position.set(1,5,5);
  const pointLight1 = new THREE.PointLight( 0xffffff, 10,100 );
  pointLight1.position.set(1,5,-5);
  camera.position.set(1,5,5);
  scene.add(pointLight1);
  scene.add(pointLight);
  
  const sphereSize = 0.1;
  const pointLightHelper = new THREE.PointLightHelper( pointLight, sphereSize );
  scene.add( pointLightHelper );
  const pointLightHelper1 = new THREE.PointLightHelper( pointLight1, sphereSize );
  scene.add( pointLightHelper1 );
  var phongMat = new THREE.MeshPhongMaterial({color: col, 
                     specular: col});
				
      var loader = new GLTFLoader();
      loader.load( './mesh/coffeeMug.glb?v=1584360698775', async function ( gltf ) {
 							
	    const model = gltf.scene;
		for (let i = 0; i < model.children.length; i++) {
		  const child = model.children[i];
          if (child.isMesh){
		    child.material=phongMat;
			child.geometry.computeVertexNormals();
		  }
		}
		scene.add( model );
        mixer = new THREE.AnimationMixer( model );
        await renderer.compileAsync( model, camera, scene );
        gltf.animations.forEach( ( clip ) => {
          
            mixer.clipAction( clip ).play();
          
        } );

      } );

  renderer = new THREE.WebGLRenderer( { antialias: true } );
  renderer.setPixelRatio( window.devicePixelRatio );
  renderer.setSize( window.innerWidth, window.innerHeight );
  container.appendChild( renderer.domElement );
  
  controls = new OrbitControls( camera, renderer.domElement );
  controls.minDistance = 2;
  controls.maxDistance = 100
  controls.target.set( 0, 0, - 0.2 );
  controls.update();

  window.addEventListener( 'resize', onWindowResize, false );

}

function onWindowResize() {

  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();

  renderer.setSize( window.innerWidth, window.innerHeight );

}

//

function Animate() {
  
  requestAnimationFrame( Animate );
  
  var delta = clock.getDelta();
  
  if ( mixer ) mixer.update( delta );

  renderer.render( scene, camera );

}
export {InitScene, Animate};