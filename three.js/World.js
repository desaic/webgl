import * as THREE from './js/three.module.js';
import { OrbitControls } from './js/OrbitControls.js';

/// unit m
class World{
    constructor(){
        this.scene = new THREE.Scene();
        this.renderer = new THREE.WebGLRenderer();
        this.renderer.setSize( window.innerWidth, window.innerHeight );
        document.body.appendChild( this.renderer.domElement );

        const  showLightPos = true;
        this.SetupLights(showLightPos);
        this.SetupCamera();

        // Cube
        var geometry = new THREE.BoxGeometry( 1, 1, 1 );
        var material = new THREE.MeshPhongMaterial( { specular: 0x808080, color:0xCCDDFF, shininess:1000 } );
        this.cube = new THREE.Mesh( geometry, material );
        this.scene.add( this.cube );

        // Torus knot
        geometry = new THREE.TorusKnotGeometry( 10, 3, 100, 16 );
        material =  new THREE.MeshPhongMaterial( { specular: 0x808080, color:0xCCDDFF, shininess:1000 } );
        const torusKnot = new THREE.Mesh( geometry, material );
        this.scene.add( torusKnot );
        
        const startButton = document.getElementById( 'startButton' );
        startButton.world = this;
        startButton.addEventListener( 'click', function (){this.world.PlaySound(); });

        this.scene.background = new THREE.Color( 0x706050 );

        this.frameCnt = 0;
    }

    PlaySound() {
        // create an AudioListener and add it to the camera
        const listener = new THREE.AudioListener();
        this.camera.add( listener );
        // create a global audio source
        const sound = new THREE.Audio( listener );
        const songElement = document.getElementById( 'song' );
        sound.setMediaElementSource( songElement );
        songElement.play();
        this.scene.add(sound);
        
        // // load a sound and set it as the Audio object's buffer
        // const loader = new THREE.AudioLoader();
        // // load a resource
        // loader.load(
        //     // resource URL
        //     'audio/ambient_ocean.ogg',

        //     // onLoad callback
        //     function ( audioBuffer ) {
        //         // set the audio object buffer to the loaded object
        //         oceanAmbientSound.setBuffer( audioBuffer );

        //         // play the audio
        //         oceanAmbientSound.play();
        //     },

        //     // onProgress callback
        //     function ( xhr ) {
        //         console.log( (xhr.loaded / xhr.total * 100) + '% loaded' );
        //     },

        //     // onError callback
        //     function ( err ) {
        //         console.log( 'An error happened' );
        //     }
        // );
    }

    SetupCamera(){
        const camZnear = 0.1;
        const camZfar = 100;
        this.camera = new THREE.PerspectiveCamera( 75, window.innerWidth / window.innerHeight, camZnear, camZfar );
        this.camControl = new OrbitControls(this.camera, this.renderer.domElement);
        this.camControl.minDistance = camZnear;
        this.camControl.maxDistance = camZfar;
        this.camControl.maxPolarAngle = Math.PI / 2; 
        this.camControl.screenSpacePanning = false;
        this.camera.position.x = 0;  
        this.camera.position.y = 1;  
        this.camera.position.z = 2;
    }

    SetupLights(showLightPos){
    
        this.light = new THREE.PointLight( 0xDDDDDD, 1.0 );
        this.light.position.set( -2,5,-2  );
        this.light1 = new THREE.PointLight( 0xCCCCCC, 1.0 );
        this.light1.position.set(3,2,2  );
        this.light2 = new THREE.DirectionalLight( 0xCCCCCC, 0.4 );
        this.light2.position.set(30,20,40  );
        this.light3 = new THREE.DirectionalLight( 0xCCCCCC, 0.4 );
        this.light3.position.set(-30,20,-40  );

        this.ambientLight = new THREE.AmbientLight( 0x202020 );
        const sphereSize = 0.1;
        const pointLightHelper1 = new THREE.PointLightHelper( this.light1, sphereSize );
        const pointLightHelper = new THREE.PointLightHelper( this.light, sphereSize );
        const pointLightHelper2 = new THREE.DirectionalLightHelper( this.light2, sphereSize );
        const pointLightHelper3 = new THREE.DirectionalLightHelper( this.light3, sphereSize );

        this.scene.add( this.light );
        this.scene.add( this.light1 );       
        this.scene.add( this.light2 );
        this.scene.add( this.light3 );
        if(showLightPos){
            this.scene.add( pointLightHelper );
            this.scene.add( pointLightHelper1 );
            this.scene.add( pointLightHelper2 );
            this.scene.add( pointLightHelper3 );    
        }
        this.scene.add( this.ambientLight );
    }

    render(){
        this.frameCnt++;
        this.camControl.update();
        this.renderer.render( this.scene, this.camera );
    }
}

export {World};