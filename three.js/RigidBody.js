
import * as THREE from './js/three.module.js';

class RigidBody{
    constructor(mesh){
        this.x = new THREE.Vector3();
        this.x.copy(mesh.position);
        this.v = new THREE.Vector3(0,0,0);        
        this.R = new THREE.Matrix4();
        this.R.makeRotationFromEuler(mesh.rotation);
        this.w = new THREE.Vector3(0,0,0);
        this.mesh = mesh;
    }
    UpdateMesh(){
        this.mesh.position.copy(this.x);
        this.mesh.rotation.setFromMatrix(this.R);
    }
}

export {RigidBody};