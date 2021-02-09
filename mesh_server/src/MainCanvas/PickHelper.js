import * as THREE from 'three'

// PICKER
//https://threejsfundamentals.org/threejs/lessons/threejs-picking.html
export default class PickHelper {
	constructor() {
		this.raycaster = new THREE.Raycaster();
		this.pickedObject = null;
		this.pickedObjectSavedColor = 0;
	}
	pick(normalizedPosition, scene, camera, meshes) {
		// restore the color if there is a picked object
		if (this.pickedObject) {
			if(this.pickedObject.material.emissive){
				this.pickedObject.material.emissive.setHex(this.pickedObjectSavedColor);
			}
			this.pickedObject = undefined;
		}

		// cast a ray through the frustum
		this.raycaster.setFromCamera(normalizedPosition, camera);
		const intersectedObjects = this.raycaster.intersectObjects(meshes);

		if (intersectedObjects.length) {
			// pick the first object. It's the closest one
			this.pickedObject = intersectedObjects[0].object;
			if(this.pickedObject.material.emissive){
				this.pickedObjectSavedColor = this.pickedObject.material.emissive.getHex();
				this.pickedObject.material.emissive.setHex(0x113333);
			}
			return intersectedObjects[0].object;
		}
	}
}