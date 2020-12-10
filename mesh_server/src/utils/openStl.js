import { snapMeshToBasePlane } from './snapMeshToBasePlane'

import * as THREE from 'three'
import { STLLoader } from 'three/examples/jsm/loaders/STLLoader.js'

export const openStl = (file) => {
	const loader = new STLLoader();
	var geometry = loader.parse(file);
	const material = new THREE.MeshPhongMaterial({ color: 0xff5533, specular: 0x111111, shininess: 200 });
	const mesh = new THREE.Mesh(geometry, material);
	mesh.castShadow = true;
	mesh.receiveShadow = true;
	snapMeshToBasePlane(mesh);
	return mesh;
}