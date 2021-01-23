import { saveAs } from 'file-saver';

export const saveMeshList = (meshes) => {
	const jsonArray = [];
	meshes.forEach(element => {
		jsonArray.push({
			uuid: element.uuid,
			name: element.name,
			position: {
				x: element.position.x,
				y: element.position.y,
				z: element.position.z,
			},
			rotation: {
				x: element.rotation.x,
				y: element.rotation.y,
				z: element.rotation.z,
			},
		})
	})
	const blob = new Blob([JSON.stringify(jsonArray)], { type: "application/json;charset=utf-8" })
	saveAs(blob, "mesh_list.json");
}