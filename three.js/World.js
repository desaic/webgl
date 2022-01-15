import * as THREE from './js/three.module.js';
import { OrbitControls } from './js/OrbitControls.js';

/// unit m
class World{
    constructor(){
        this.scene = new THREE.Scene();
        this.renderer = new THREE.WebGLRenderer();
        this.renderer.setSize( window.innerWidth, window.innerHeight );
        document.body.appendChild( this.renderer.domElement );

        const  showLightPos = false;
        this.SetupLights(showLightPos);
        this.SetupCamera();
        
        var geometry = new THREE.BoxGeometry( 1, 1, 1 );
        var material = new THREE.MeshPhongMaterial( { specular: 0x808080, color:0xCCDDFF, shininess:1000 } );
        this.cube = new THREE.Mesh( geometry, material );
        this.scene.add( this.cube );
        this.scene.background = new THREE.Color( 0x706050 );

        this.frameCnt = 0;
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