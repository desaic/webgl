import * as THREE from 'three'
import { addShadowedLight } from '../utils/addShadowedLight'
export default class World
{
    constructor() {
        const fov = 50;
        const aspect=(window.innerWidth / window.innerHeight);
        const near = 0.01;
        const far = 3000;
        this.camera = new THREE.PerspectiveCamera(fov, aspect, near, far);
		this.camera.position.set(0, 0, 180);
        this.camera.lookAt(0, 200, 0);
        
        this.scene = new THREE.Scene();
		this.scene.background = new THREE.Color(0xcccccc);

        this.meshes = []
        this.images = []
        
		this.ground = new THREE.Mesh(
			//unit is mm
			new THREE.PlaneBufferGeometry(400, 250),
			new THREE.MeshPhongMaterial({ color: 0x999999, specular: 0x101010 })
		);
		this.ground.rotation.x = - Math.PI / 2
		this.ground.position.y = 0 // = - 0.5;
		this.ground.receiveShadow = true
		//this.scene.add(this.ground)        
		
		this.canvas = document.createElement('canvas');
		var context = this.canvas.getContext('2d');
		this.canvas.width = 800;
		this.canvas.height = 600;
		context = this.canvas.getContext('2d');
		context.fillStyle='green';
		context.fillRect(0,0,this.canvas.width,this.canvas.height);
		var texture = new THREE.Texture(this.canvas);
		var screenGeom = new THREE.BoxBufferGeometry( 100, 100, 100 );
		var screenmat = new THREE.MeshPhongMaterial( { color: 0xeeeeee, specular: 0x101010, map:texture} );
		this.screen = new THREE.Mesh( screenGeom, screenmat );
		screenmat.map.needsUpdate = true;
		//this.screen.position.y = 50;
		this.scene.add( this.screen );

		//lights
		this.scene.add(new THREE.HemisphereLight(0x443333, 0x111122))
		addShadowedLight(1, 10, 50, 0xdddddd, 1, this.scene)
		addShadowedLight(0.5, 50, -55, 0xdd8800, 1, this.scene)
	}
}