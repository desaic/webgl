
import * as THREE from 'three'


export const addShadowedLight = (x, y, z, color, intensity, scene) => {
	const directionalLight = new THREE.DirectionalLight(color, intensity);
	directionalLight.position.set(x, y, z);
	scene.add(directionalLight);
	directionalLight.castShadow = true;
	const d = 400;
	directionalLight.shadow.camera.left = - d;
	directionalLight.shadow.camera.right = d;
	directionalLight.shadow.camera.top = d;
	directionalLight.shadow.camera.bottom = - d;
	directionalLight.shadow.camera.near = 1;
	directionalLight.shadow.camera.far = 400;
	directionalLight.shadow.bias = - 0.002;
}