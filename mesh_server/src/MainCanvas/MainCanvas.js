import React from 'react'
import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls'
import { TransformControls } from 'three/examples/jsm/controls/TransformControls.js'
import PickHelper from './PickHelper'
import './MainCanvas.scss'
import { addShadowedLight } from '../utils/addShadowedLight'
import { MeshStateHistory } from '../utils/MeshStateHistory'

const MESH_HISTORY = new MeshStateHistory();
const PICK_HELPER = new PickHelper();
const WEB_SOCKET = new WebSocket('ws://localhost:9000/')

WEB_SOCKET.onopen = function() {
	WEB_SOCKET.send('websocket client\r\n');
};

let CURRENT_CAMERA
let SCENE
let RENDERER
let CONTROL
let ORBIT

let pickPosition = { x: 0, y: 0 };
let meshes = []
let selectedMesh = null

export default class MainCanvas extends React.Component {
	canvasRef = React.createRef()

	componentDidMount() {
	
		RENDERER = new THREE.WebGLRenderer({
			canvas: this.canvasRef.current,
		});
		RENDERER.setPixelRatio(window.devicePixelRatio)
		RENDERER.setSize(window.innerWidth, window.innerHeight)
		RENDERER.shadowMap.enabled = true

		CURRENT_CAMERA = new THREE.PerspectiveCamera(50, (window.innerWidth / window.innerHeight), 0.01, 30000)
		CURRENT_CAMERA.position.set(100, 150, 300)
		CURRENT_CAMERA.lookAt(0, 200, 0)

		SCENE = new THREE.Scene()
		SCENE.background = new THREE.Color(0xcccccc)

		// Ground
		const plane = new THREE.Mesh(
			//unit is mm
			new THREE.PlaneBufferGeometry(400, 250),
			new THREE.MeshPhongMaterial({ color: 0x999999, specular: 0x101010 })
		);
		plane.rotation.x = - Math.PI / 2
		plane.position.y = 0 // = - 0.5;
		plane.receiveShadow = true
		SCENE.add(plane)

		//lights
		SCENE.add(new THREE.HemisphereLight(0x443333, 0x111122))
		addShadowedLight(1, 200, 1, 0xdddddd, 1.35, SCENE)
		addShadowedLight(0.5, 100, - 1, 0xdd8800, 1, SCENE)

		// Orbit controls
		ORBIT = new OrbitControls(CURRENT_CAMERA, RENDERER.domElement)
		ORBIT.update()
		ORBIT.addEventListener('change', this.updateCanvasRender)
		CONTROL = new TransformControls(CURRENT_CAMERA, RENDERER.domElement)
		CONTROL.addEventListener('change', this.updateCanvasRender)
		CONTROL.addEventListener('dragging-changed', function (event) {
			ORBIT.enabled = !event.value
			if(event.value && selectedMesh) {
				MESH_HISTORY.setTransientMeshState(selectedMesh)
			}
			if(!event.value && selectedMesh) {
				MESH_HISTORY.recordMeshStateChange(selectedMesh)
			}
		});
		//* Default box
		const geometry = new THREE.BoxBufferGeometry(50, 50, 50)
		const material = new THREE.MeshPhongMaterial({ color: 0xc0b0cc, specular: 0x111111, shininess: 200 })
		let mesh = new THREE.Mesh(geometry, material)
		mesh.position.set(0, 25, 0)
		mesh.castShadow = true
		mesh.receiveShadow = true

		this.addMesh("DefaultBox", mesh)
		SCENE.add(CONTROL)
		
		this.bindEventListeners()
		this.clearPickPosition()
		this.updateCanvasRender()
		
		WEB_SOCKET.onmessage = (message) => {
			this.parseSocketMsg(message.data);
		};
	}

	shouldComponentUpdate() {
		return false
	}

	bindEventListeners = () => {
		const canvasElement = this.canvasRef.current
		window.addEventListener('resize', this.updateCanvasRender, false)
		canvasElement.addEventListener('click', this.selectObj)
		canvasElement.addEventListener('mousemove', this.setPickPosition)
		canvasElement.addEventListener('mouseout', this.clearPickPosition)
		canvasElement.addEventListener('mouseleave', this.clearPickPosition)

		canvasElement.addEventListener('keydown', (event) => {
			switch (event.key) {
				case 'Shift':
					CONTROL.setTranslationSnap(10);
					CONTROL.setRotationSnap(THREE.MathUtils.degToRad(15));
					CONTROL.setScaleSnap(0.25);
					break;
				case 't':
					this.setMode("translate");
					break;
				case 'r':
					this.setMode("rotate");
					break;
				case 's':
					this.setMode("scale");
					break;
				case 'Delete':
					this.deleteSelected();
					break;
				case 'Escape':
					this.deselectMesh();
					break;
				case 'd':
					this.duplicateMesh(selectedMesh);
					break;
				default:
			}
		});
		canvasElement.addEventListener('keyup', (event) => {
			switch (event.key) {
				case 'Shift':
					CONTROL.setTranslationSnap(null);
					CONTROL.setRotationSnap(null);
					CONTROL.setScaleSnap(null);
					break;
				default:
			}
		});
	}

	setPickPosition = (event) => {
		pickPosition.x = (event.clientX / window.innerWidth) * 2 - 1
		pickPosition.y = - (event.clientY / window.innerHeight) * 2 + 1
		this.updateCanvasRender()
	}

	clearPickPosition = () => {
		pickPosition.x = -100000
		pickPosition.y = -100000
	}

	createMesh = (meshId, trigs)=>{
		//make a debug mesh.
		const geometry = new THREE.BufferGeometry();
		geometry.setAttribute( 'position', new THREE.BufferAttribute( trigs, 3 ) );
		//geometry.computeFaceNormals();
		geometry.computeVertexNormals();
		const material = new THREE.MeshPhongMaterial( { color: 0x999999, specular: 0x101010 } );
		const mesh = new THREE.Mesh( geometry, material );
		this.addMesh(meshId.toString(), mesh);
	}

	parseBlob = (blob) => {
		new Response(blob).arrayBuffer()
		.then(buf => {
			//|msg type 2 bytes| meshid 2 bytes | numTrigs 4 bytes
			//does not support more than 4 billion trigs.
			const HEADER_BYTES = 8;
			if(buf.length<HEADER_BYTES){
				console.log("message too small");
				return;
			}
			const header = new Uint16Array(buf,0,4);
			const cmd = header[0];
			const meshId = header[1];
			///\TODO bad hack. not sure best way to do this.
			const numTrigs = header[2] + 65536*header[3];
			const fvals = new Float32Array(buf,HEADER_BYTES);
			this.createMesh(meshId, fvals);
		});
	}

	//parse mesh received from web socket
	//and add to mesh list.
	parseSocketMsg = (msg) =>{
		if(msg instanceof Blob){
			this.parseBlob(msg);
		}else if(typeof msg === 'string'){
			console.log(msg);
		}
	}

	selectObj = () => {
		const sM = PICK_HELPER.pick(pickPosition, SCENE, CURRENT_CAMERA, meshes)
		if (sM) {
			selectedMesh = sM
			CONTROL.attach(selectedMesh)
		}
		this.updateCanvasRender()
	}

	getMeshList = () => meshes

	setMode = (mode) => {
		CONTROL.setMode(mode)
	}

	deleteSelected = () => {
		if (selectedMesh) {
			// Remove the object from scene
			MESH_HISTORY.clearHistory(selectedMesh.uuid)
			selectedMesh.geometry.dispose()
			selectedMesh.material.dispose()
			SCENE.remove(selectedMesh)
			const i = meshes.indexOf(selectedMesh)
			if (i > -1) {
				meshes.splice(i, 1)
			}
			selectedMesh = null
			// Remove controls from scene
			CONTROL.detach()
		}
		this.updateCanvasRender()
	}
	
	deselectMesh = () => {
		if(selectedMesh) {
			CONTROL.detach()
			selectedMesh = null
			this.updateCanvasRender()
		}
	}

	duplicateMesh = () => {
		const mesh = selectedMesh
		// Clone geometry and material so that they have separate uuid's
		// If you do selectedMesh.clone(), then orig. and new mesh geometry's
		// have same uuid and raycaster will group them together.
		let duplicate = new THREE.Mesh(mesh.geometry.clone(), mesh.material.clone())
		//remember to disable emission
		duplicate.material.emissive.setHex(0x0)
		duplicate.rotation.copy(mesh.rotation) // Copy rotation
		duplicate.position.copy(mesh.position) // Copy position
		duplicate.scale.copy(mesh.scale) // Copy position
		// Translate the mesh (in object space)
		duplicate.translateX(10)
		duplicate.translateZ(10)
	
		// Get the duplicate's name
		let name = ""
		const i = meshes.indexOf(selectedMesh)
		if (i > -1) {
			name = meshes[i].name
		}
	
		this.addMesh(name, duplicate)
	}

	addMesh = (name, mesh) => {
		if (mesh !== undefined) {
			SCENE.add(mesh)
			mesh.name = name
			meshes.push(mesh)
			CONTROL.attach(mesh)
			selectedMesh = mesh
		} else {
			alert("The STL you uploaded is not valid")
			return;
		}
		this.updateCanvasRender()
	}

	updateCanvasRender = () => {
		const aspect = window.innerWidth / window.innerHeight
		CURRENT_CAMERA.aspect = aspect
		CURRENT_CAMERA.updateProjectionMatrix()
		RENDERER.setSize(window.innerWidth, window.innerHeight)
		//PICK_HELPER.pick(pickPosition, SCENE, CURRENT_CAMERA,meshes)
		RENDERER.render(SCENE, CURRENT_CAMERA)
		this.props.onSelectedMeshDataChange(selectedMesh)
	}

	handlePositionXChange = (event) => {
		MESH_HISTORY.setTransientMeshState(selectedMesh)
		selectedMesh.position.x = event.target.value
		MESH_HISTORY.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}

	handlePositionYChange = (event) => {
		MESH_HISTORY.setTransientMeshState(selectedMesh)
		selectedMesh.position.y = event.target.value
		MESH_HISTORY.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}
	handlePositionZChange = (event) => {
		MESH_HISTORY.setTransientMeshState(selectedMesh)
		selectedMesh.position.z = event.target.value
		MESH_HISTORY.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}

	handleRoatationXChange = (event) => {
		MESH_HISTORY.setTransientMeshState(selectedMesh)
		selectedMesh.rotation.x = event.target.value
		MESH_HISTORY.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}
	handleRoatationYChange = (event) => {
		MESH_HISTORY.setTransientMeshState(selectedMesh)
		selectedMesh.rotation.y = event.target.value
		MESH_HISTORY.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}
	handleRoatationZChange = (event) => {
		MESH_HISTORY.setTransientMeshState(selectedMesh)
		selectedMesh.rotation.z = event.target.value
		MESH_HISTORY.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}

	handleUndoAction = () => {
		const meshStateChange = MESH_HISTORY.popHistory()
		if(!meshStateChange) {
			return
		}
		meshes.forEach(mesh => {
			if(meshStateChange.id === mesh.uuid) {
				MESH_HISTORY.applyStateToMesh(mesh, meshStateChange.previousState)
			}
		})
		this.updateCanvasRender()
	}

	render() {
		return (
			<canvas id="mainCanvas" ref={this.canvasRef}></canvas>
		)
	}

}

