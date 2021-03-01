import * as THREE from 'three'
import { addShadowedLight } from '../utils/addShadowedLight'
export default class World
{
    constructor() {
        const fov = 50;
        const aspect=(window.innerWidth / window.innerHeight);
        const near = 0.01;
        const far = 2000;
        this.camera = new THREE.PerspectiveCamera(fov, aspect, near, far);
		this.camera.up.set(0,0,1);
		this.camera.position.set(0, 400, 150);
        this.camera.lookAt(200, 125, 0);

        this.scene = new THREE.Scene();
		this.scene.background = new THREE.Color(0xaaaaaa);

        this.meshes = []
        this.images = []
        
		const planeSize = [400,250];

		this.ground = new THREE.Mesh(
			//unit is mm
			new THREE.PlaneBufferGeometry(planeSize[0], planeSize[1]),
			new THREE.MeshPhongMaterial({ color: 0x999999, specular: 0x101010 })
		);
		this.ground.position.x = 0
		this.ground.position.y = 0 
		this.ground.position.z = 0 // = - 0.5;
		this.ground.receiveShadow = true
        this.scene.add(this.ground)
        
		//lights
		this.scene.add(new THREE.HemisphereLight(0x443333, 0x111122))
		addShadowedLight(0, 10, 100, 0xdddddd, 1.35, this.scene)
		addShadowedLight(200, 0, 100, 0xdd8800, 1, this.scene)
	}
}