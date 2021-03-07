import * as THREE from 'three'


export const snapMeshToBasePlane = (mesh) => {
	// Snap mesh to floor
	// Moves the mesh up or down based on its bounding box
	mesh.geometry.center();	// Center the geometry (and origin) about the center of the bounding box
	var boundBox = new THREE.Box3().setFromObject(mesh);
	mesh.position.set(0, 0, boundBox.getSize().z / 2);
	return mesh;
}