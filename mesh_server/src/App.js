import React from 'react'
import MainContextCanvas from './MainContextCanvas/MainContextCanvas'
import Info from './Info/Info'
import MenuBar from './MenuBar/MenuBar'
import MeshInfo from './MeshInfo/MeshInfo'
import { openStl } from './utils/openStl'
import { saveMeshList } from './utils/saveMeshList'
import { useState } from 'react'

import './App.scss'

const mainContextCanvasRef = React.createRef();

const handleFileOpen = (event) => {
	const { files, value } = event.target
	const stlFile = files[0]
	const filename = value.replace(/.*[\/\\]/, '')
	let reader = new FileReader()
	reader.readAsArrayBuffer(stlFile)
	reader.onload = function () {
		mainContextCanvasRef.current.addMesh(filename, openStl(reader.result));
	}
}

const handleSaveMeshList = () => {
	saveMeshList(mainContextCanvasRef.current.getMeshList())
}

function App() {
	const [meshName, setMeshName] = useState('')

	const [meshPositionX, setMeshPositionX] = useState(null)
	const [meshPositionY, setMeshPositionY] = useState(null)
	const [meshPositionZ, setMeshPositionZ] = useState(null)

	const [meshRotationX, setMeshRotationX] = useState(null)
	const [meshRotationY, setMeshRotationY] = useState(null)
	const [meshRotationZ, setMeshRotationZ] = useState(null)
	const [showMeshInfo, setShowMeshInfo] = useState(false)

	const [boundaryExceded, setBoundaryExceded] = useState(false)

	const handleSelectedMeshDataChange = (selectedMesh) => {
		if(selectedMesh !== null) {
			const {name, position, rotation} = selectedMesh
			setMeshName(name)
			setMeshPositionX(position.x)
			setMeshPositionY(position.y)
			setMeshPositionZ(position.z)
			setMeshRotationX(rotation.x)
			setMeshRotationY(rotation.y)
			setMeshRotationZ(rotation.z)
			setShowMeshInfo(true)
		} else if (showMeshInfo){
			setShowMeshInfo(false)
		}
	}

	const handleUndoAction = () => {
		if(mainContextCanvasRef.current) {
			mainContextCanvasRef.current.handleUndoAction()
		}
	}

	return (
		<div className="App">
			<MainContextCanvas 
				ref={mainContextCanvasRef}
				onSelectedMeshDataChange={handleSelectedMeshDataChange}
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
			{showMeshInfo && mainContextCanvasRef.current && (
				<MeshInfo
					meshName={meshName}
					meshPositionX={meshPositionX}
					meshPositionY={meshPositionY}
					meshPositionZ={meshPositionZ}
					meshRotationX={meshRotationX}
					meshRotationY={meshRotationY}
					meshRotationZ={meshRotationZ}
					onPositionXChange={mainContextCanvasRef.current.handlePositionXChange}
					onPositionYChange={mainContextCanvasRef.current.handlePositionYChange}
					onPositionZChange={mainContextCanvasRef.current.handlePositionZChange}
					onRoatationXChange={mainContextCanvasRef.current.handleRoatationXChange}
					onRoatationYChange={mainContextCanvasRef.current.handleRoatationYChange}
					onRoatationZChange={mainContextCanvasRef.current.handleRoatationZChange}
				/>
			)}
		</div>
	);
}

export default App;