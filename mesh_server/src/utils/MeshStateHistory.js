export class MeshStateHistory {
	constructor(){
		///a list of mesh history states for each mesh 
		this.hist = [];
	}

	removeMesh = (mesh) => {
		this.hist.splice(mesh.idx, 1)
	}

	pop = (mesh) => {
		var list = this.hist[mesh.idx];
		if(list.length>1){
			list.pop()
		}		
	}

	top = (mesh)=>{
		var list = this.hist[mesh.idx];
		if(list.length>0){
			return list[list.length-1];
		}else{
			return {pos:[0,0,0],rot:[0,0,0]};
		}
	}

	addMesh = (mesh) => {
		const meshState = this.extractState(mesh)
		const idx = mesh.idx;
		if(this.hist.length>idx){
			this.hist[idx].push(meshState);
		}else{
			this.hist.push([meshState]);
		}
	}

	apply = (mesh, st) => {
		if(mesh){
			mesh.position.set(st.pos[0], st.pos[1], st.pos[2]);
			mesh.rotation.set(st.rot[0], st.rot[1], st.rot[2]);
		}
	}

	extractState = (mesh) => {
		if(mesh) {
			return ({
				pos: [mesh.position.x, mesh.position.y,mesh.position.z],
				rot: [mesh.rotation.x, mesh.rotation.y,mesh.rotation.z]
				})
		}
		return {}
	}

}