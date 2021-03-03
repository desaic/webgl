import React from 'react'
import MainCanvas from './MainCanvas/MainCanvas'
import Info from './Info/Info'
import MenuBar from './MenuBar/MenuBar'
import MeshInfo from './MeshInfo/MeshInfo'
import { openStl } from './utils/openStl'
import { saveMeshList } from './utils/saveMeshList'
import { useState } from 'react'

import './App.scss'

const mainRef = React.createRef();

const GetFileExtension = (name) => {
	var a = name.split(".");
	if( a.length === 1 || ( a[0] === "" && a.length === 2 ) ) {
		return "";
	}
	return a.pop().toLowerCase(); 
}

const handleFileOpen = (event) => {
	const { files, value } = event.target
	const stlFile = files[0]
	const filename = value.replace(/.*[/\\]/, '')
	let reader = new FileReader()
	try{
		reader.onload = function () {
			var ext = GetFileExtension(filename);
			if(ext == 'stl'){
				var mesh =openStl(reader.result);
				mesh.name = filename;
				mainRef.current.addMesh(mesh);
			}
		}
		reader.readAsArrayBuffer(stlFile)
	}catch(err){
		console.log(err.message);
	}
}

const handleSaveMeshList = () => {
	saveMeshList(mainRef.current.getMeshList())
}

function App() {
	const [meshName, setMeshName] = useState('')

	const [meshTrans, setMeshTrans] = useState({pos:[0,0,0], rot:[0,0,0]})
	const [boundaryExceded, setBoundaryExceded] = useState(false)

	const showMeshTrans = (selectedMesh) => {
		if(selectedMesh == null) {
			return;
		}
		const {name, position, rotation} = selectedMesh
		setMeshName(name)
		const meshT = {pos:[position.x,position.y, position.z],
			rot: [rotation.x,rotation.y,rotation.z]}
		setMeshTrans(meshT)
	}

	const handleUndoAction = () => {
		if(mainRef.current) {
			mainRef.current.handleUndoAction()
		}
	}

	return (
		<div className="App">
			<MainCanvas 
				ref={mainRef}
				showMeshTrans={showMeshTrans}
				onBoundriesExceded={setBoundaryExceded}
			/>
			<Info
				boundaryExceded={boundaryExceded}
			/>
			<MenuBar
				onFileOpen={handleFileOpen}
				onSaveMeshList={handleSaveMeshList}
				onUndo={handleUndoAction}
			/>
			{mainRef.current && (
				<MeshInfo
					meshName={meshName}
					meshTrans={meshTrans}
					onMeshTrans={mainRef.current.onTransChange}
				/>
			)}
		</div>
	);
}

export default App;