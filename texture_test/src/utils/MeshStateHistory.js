let transientMeshState = null
let meshHistoryList = []

export class MeshStateHistory {
	clearHistory = (meshId) => {
		let newMeshHistoryList = []
		meshHistoryList.forEach(item => {
			if(item.id !== meshId) {
				newMeshHistoryList.push(item)
			}
		})
		meshHistoryList = newMeshHistoryList
	}

	popHistory = () => {
		return meshHistoryList.pop()
	}

	setTransientMeshState = (mesh) => {
		transientMeshState = this.extractMeshState(mesh)
	}

	recordMeshStateChange = (mesh) => {
		const meshStateChange = {
			id: mesh.uuid,
			previousState: transientMeshState,
			nextState: this.extractMeshState(mesh),
		}
		meshHistoryList.push(meshStateChange)
		transientMeshState = null
	}

	applyStateToMesh = (mesh, meshState) => {
		if(mesh.position && meshState.position) {
			mesh.position.x = meshState.position.x
			mesh.position.y = meshState.position.y
			mesh.position.z = meshState.position.z
		}
		if(mesh.rotation && meshState.rotation) {
			mesh.rotation.x = meshState.rotation.x
			mesh.rotation.y = meshState.rotation.y
			mesh.rotation.z = meshState.rotation.z
		}
	}

	extractMeshState = (mesh) => {
		if(mesh) {
			return ({
				name: mesh.name,
				position: {
					x: mesh.position.x,
					y: mesh.position.y,
					z: mesh.position.z,
				},
				rotation: {
					x: mesh.rotation.x,
					y: mesh.rotation.y,
					z: mesh.rotation.z,
				}
			})
		}
		return {}
	}

}