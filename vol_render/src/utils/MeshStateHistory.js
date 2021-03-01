export class MeshStateHistory {
	constructor(){
		///maps from mesh id to list of states.
		this.hist = new Map();
	}
	///\param meshId string of uuid of mesh
	///can also be changed to use a simple int id.
	removeMesh = (mesh) => {
		this.hist.delete(mesh.uuid);
	}

	pop = (mesh) => {
		var list = this.hist.get(mesh.uuid);
		if(list.length>1){
			list.pop()
		}		
	}

	top = (mesh)=>{
		var list = this.hist.get(mesh.uuid);
		if(list.length>0){
			return list[list.length-1];
		}else{
			return {pos:[0,0,0],rot:[0,0,0]};
		}
	}

	addMeshState = (mesh) => {
		const meshState = {
			id: mesh.uuid,
			state: this.extractMeshState(mesh),
		}
		var list = this.hist.get(mesh.uuid);
		if(list){
			list.push(meshState);
		}else{
			this.hist.set(mesh.uuid, [meshState]);
		}		
	}

	applyMeshState = (mesh, st) => {
		if(mesh){
			mesh.position.set(st.pos[0], st.pos[1], st.pos[2]);
			mesh.rotation.set(st.rot[0], st.rot[1], st.rot[2]);
		}
	}

	extractMeshState = (mesh) => {
		if(mesh) {
			return ({
				pos: [mesh.position.x, mesh.position.y,mesh.position.z],
				rot: [mesh.rotation.x, mesh.rotation.y,mesh.rotation.z]
				})
		}
		return {}
	}

}