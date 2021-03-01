import React from 'react'
import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls'
import { TransformControls } from 'three/examples/jsm/controls/TransformControls.js'
import PickHelper from './PickHelper'
import './MainCanvas.scss'
import { MeshStateHistory } from '../utils/MeshStateHistory'
import World from './World'

const world = new World();
const pickHelper = new PickHelper();
const webSocket = new WebSocket('ws://localhost:9000/')
const hist = new MeshStateHistory();

let renderer;
let control;
let orbit;
let selectedMesh = null;
let pickPosition = { x: 0, y: 0 };

webSocket.onopen = function() {
	webSocket.send('websocket client\r\n');
};

export default class MainCanvas extends React.Component {
	canvasRef = React.createRef()

	componentDidMount() {
		renderer = new THREE.WebGLRenderer({
			canvas: this.canvasRef.current,
		});
		renderer.setPixelRatio(window.devicePixelRatio)
		renderer.setSize(window.innerWidth, window.innerHeight)
		renderer.shadowMap.enabled = true
		// Orbit controls
		orbit = new OrbitControls(world.camera, renderer.domElement)
		orbit.update()
		orbit.addEventListener('change', this.render3D)
		control = new TransformControls(world.camera, renderer.domElement)
		control.addEventListener('change', this.meshControlChanged)
		control.addEventListener('dragging-changed', function (event) {
			//event.value true when starting drag. false when existing a drag.
			orbit.enabled = !event.value
			//only record position at end of drag.
			if(!event.value && selectedMesh) {
				hist.addMeshState(selectedMesh)
			}
		});

		world.scene.add(control)
		
		this.bindEventListeners()
		this.render3D()
		
		webSocket.onmessage = (message) => {
			this.parseSocketMsg(message.data);
		};
	}

	shouldComponentUpdate() {
		return false
	}

	meshControlChanged=(event)=>{
		this.props.showMeshTrans(selectedMesh);
		this.render3D();
	}

	bindEventListeners = () => {
		const canvasElement = this.canvasRef.current
		window.addEventListener('resize', this.onWindowResize)
		canvasElement.addEventListener('click', this.selectObj)
		canvasElement.addEventListener('mousemove', this.setPickPosition)

		canvasElement.addEventListener('keydown', (event) => {
			switch (event.key) {
				case 'Shift':
					control.setTranslationSnap(10);
					control.setRotationSnap(THREE.MathUtils.degToRad(15));
					control.setScaleSnap(0.25);
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
					control.setTranslationSnap(null);
					control.setRotationSnap(null);
					control.setScaleSnap(null);
					break;
				default:
			}
		});
	}

	onWindowResize = ()=>{
		renderer.setSize( window.innerWidth, window.innerHeight );

		const aspect = window.innerWidth / window.innerHeight;

		const frustumHeight = world.camera.top - world.camera.bottom;

		world.camera.left = - frustumHeight * aspect / 2;
		world.camera.right = frustumHeight * aspect / 2;

		world.camera.updateProjectionMatrix();
		this.render3D();
	}

	setPickPosition = (event) => {
		pickPosition.x = (event.clientX / window.innerWidth) * 2 - 1
		pickPosition.y = - (event.clientY / window.innerHeight) * 2 + 1
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
		const sM = pickHelper.pick(pickPosition, world.scene, world.camera, world.meshes);
		if (sM) {
			selectedMesh = sM
			control.attach(selectedMesh)
		}
		this.render3D()
	}

	setMode = (mode) => {
		control.setMode(mode)
	}

	deleteSelected = () => {
		if (selectedMesh) {
			// Remove the object from scene
			hist.removeMesh(selectedMesh)
			selectedMesh.geometry.dispose()
			selectedMesh.material.dispose()
			world.scene.remove(selectedMesh)
			const i = world.meshes.indexOf(selectedMesh)
			if (i > -1) {
				world.meshes.splice(i, 1)
			}
			selectedMesh = null
			// Remove controls from scene
			control.detach()
		}
		this.render3D()
	}
	
	deselectMesh = () => {
		if(selectedMesh) {
			control.detach()
			selectedMesh = null
			this.render3D()
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
		const i = world.meshes.indexOf(selectedMesh)
		if (i > -1) {
			name = world.meshes[i].name
		}
	
		this.addMesh(name, duplicate)
	}

	addMesh = (name, mesh) => {
		if (mesh !== undefined) {
			world.scene.add(mesh);
			mesh.name = name;
			world.meshes.push(mesh);
			control.attach(mesh);
			selectedMesh = mesh;
			hist.addMeshState(mesh);
			this.props.showMeshTrans(selectedMesh);
		} else {
			alert("invalid mesh data.");
			return;
		}
		this.render3D();
	}

	render3D = () => {
		const aspect = window.innerWidth / window.innerHeight;
		world.camera.aspect = aspect;
		world.camera.updateProjectionMatrix();
		renderer.setSize(window.innerWidth, window.innerHeight);
		renderer.render(world.scene, world.camera);
	}

	onTransChange = (val, axis) => {
		if(selectedMesh == null){
			return;
		}
		var m = selectedMesh;
		switch(axis){
		case 0:
			m.position.x = val;
			break;
		case 1:
			m.position.y = val;
			break;
		case 2:
			m.position.z = val;
			break;
		case 4:
			m.rotation.x = val;
			break;
		case 5:
			m.rotation.y = val;
			break;
		case 6:
			m.rotation.z = val;
			break;
		}
		hist.addMeshState(selectedMesh)
		this.render3D();
		//this.props.showMeshTrans(selectedMesh);
	}

	handleUndoAction = () => {
		if(selectedMesh == null){
			return;
		}
		hist.pop(selectedMesh)
		var h = hist.top(selectedMesh);
		if(!h) {
			return
		}
		hist.applyMeshState(selectedMesh, h.state)
		this.render3D();
	}

	render() {
		return (
			<canvas id="mainCanvas" ref={this.canvasRef}></canvas>
		)
	}

}

