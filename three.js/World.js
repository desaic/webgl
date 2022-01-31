import * as THREE from './js/three.module.js';
import { OrbitControls } from './js/OrbitControls.js';
import { RigidBody } from './RigidBody.js';

/// unit is meters
class World{
    constructor(){
        this.scene = new THREE.Scene();
        this.renderer = new THREE.WebGLRenderer();
        console.log(THREE.REVISION);
        this.renderer.setSize( window.innerWidth, window.innerHeight );
        document.body.appendChild( this.renderer.domElement );
        const  showLightPos = true;
        this.SetupLights(showLightPos);
        this.SetupCamera();


        // Torus knot
        var geometry = new THREE.TorusKnotGeometry( 1, 0.2, 50, 16 );
        var material =  new THREE.MeshPhongMaterial( { specular: 0x808080, color:0xCCDDFF, shininess:1000 } );
        const torusKnot = new THREE.Mesh( geometry, material );
        torusKnot.position.y=2;
        this.scene.add( torusKnot );
        
        this.AddFloor();

        const startButton = document.getElementById( 'startButton' );
        startButton.world = this;
        startButton.addEventListener( 'click', function (){this.world.PlayBGM(); });

        this.scene.background = new THREE.Color( 0x706050 );

        this.frameCnt = 0;

        //array of rigid bodies
        this.rb = [];
        this.AddCube();
    }

    AddCube(){
        // Cube
        var geometry = new THREE.BoxGeometry( 1, 1, 1 );
        var material = new THREE.MeshPhongMaterial( { specular: 0x808080, color:0xCCDDFF, shininess:1000 } );
        this.cube = new THREE.Mesh( geometry, material );
        this.cube.position.y=2;
        this.scene.add( this.cube );
        const cubeRb = new RigidBody(this.cube);
        this.rb.push(cubeRb);
    }

    AddFloor(){
        const floorMat = new THREE.MeshStandardMaterial( {
            roughness: 0.2,
            color: 0xffffff,
            metalness: 0.5,
            bumpScale: 0.0005
        } );
        const textureLoader = new THREE.TextureLoader();
        textureLoader.load( "./textures/hardwood2_diffuse.jpg", function ( map ) {

            map.wrapS = THREE.RepeatWrapping;
            map.wrapT = THREE.RepeatWrapping;
            map.anisotropy = 4;
            map.repeat.set( 10, 24 );
            map.encoding = THREE.sRGBEncoding;
            floorMat.map = map;
            floorMat.needsUpdate = true;

        } );
        textureLoader.load( "./textures/hardwood2_bump.jpg", function ( map ) {
            map.wrapS = THREE.RepeatWrapping;
            map.wrapT = THREE.RepeatWrapping;
            map.anisotropy = 4;
            map.repeat.set( 10, 24 );
            floorMat.bumpMap = map;
            floorMat.needsUpdate = true;

        } );

        const floorGeometry = new THREE.PlaneGeometry( 20, 20 );
        const floorMesh = new THREE.Mesh( floorGeometry, floorMat );
        floorMesh.receiveShadow = true;
        floorMesh.rotation.x = - Math.PI / 2.0;
        this.scene.add( floorMesh );
        
    }

    PlayBGM() {
        const overlay = document.getElementById( 'startOverlay' );
        overlay.remove();
        // create an AudioListener and add it to the camera
        const listener = new THREE.AudioListener();
        this.camera.add( listener );
        // create a global audio source
        const sound = new THREE.Audio( listener );
        const songElement = document.getElementById( 'song' );
        sound.setMediaElementSource( songElement );
        songElement.play();
        this.scene.add(sound);
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
        this.camControl.target.set(0,1,0);
        this.camera.position.x = 0;  
        this.camera.position.y = 2;  
        this.camera.position.z = 4;
        this.camera.lookAt(0,1,0);
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

    Render(){
        this.frameCnt++;
        this.camControl.update();
        this.renderer.render( this.scene, this.camera );
    }

    /// step the world by time dt seconds
    Step(dt){
    
      this.StepRb(dt);
    }

    StepRb(dt){
        for (var i = 0; i < this.rb.length; i++) {
            const rb = this.rb[i];
            rb.x += dt * rb.v;
            if(rb.y<1){

            }
        }
    }
}

export {World};