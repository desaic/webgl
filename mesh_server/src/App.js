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

const handleFileOpen = (event) => {
	const { files, value } = event.target
	const stlFile = files[0]
	const filename = value.replace(/.*[\/\\]/, '')
	let reader = new FileReader()
	reader.readAsArrayBuffer(stlFile)
	reader.onload = function () {
		mainRef.current.addMesh(filename, openStl(reader.result));
	}
}

const handleSaveMeshList = () => {
	saveMeshList(mainRef.current.getMeshList())
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
		if(mainRef.current) {
			mainRef.current.handleUndoAction()
		}
	}

	return (
		<div className="App">
			<MainCanvas 
				ref={mainRef}
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
			{showMeshInfo && mainRef.current && (
				<MeshInfo
					meshName={meshName}
					meshPositionX={meshPositionX}
					meshPositionY={meshPositionY}
					meshPositionZ={meshPositionZ}
					meshRotationX={meshRotationX}
					meshRotationY={meshRotationY}
					meshRotationZ={meshRotationZ}
					onPositionXChange={mainRef.current.handlePositionXChange}
					onPositionYChange={mainRef.current.handlePositionYChange}
					onPositionZChange={mainRef.current.handlePositionZChange}
					onRoatationXChange={mainRef.current.handleRoatationXChange}
					onRoatationYChange={mainRef.current.handleRoatationYChange}
					onRoatationZChange={mainRef.current.handleRoatationZChange}
				/>
			)}
		</div>
	);
}

export default App;