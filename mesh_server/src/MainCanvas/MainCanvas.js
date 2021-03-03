import React from 'react'
import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls'
import { TransformControls } from 'three/examples/jsm/controls/TransformControls.js'
import MeshServer from './MeshServer'
import PickHelper from './PickHelper'
import './MainCanvas.scss'
import { MeshStateHistory } from '../utils/MeshStateHistory'
import World from './World'

const meshServer = new MeshServer();
const pickHelper = new PickHelper();
const hist = new MeshStateHistory();

let renderer;
let control;
let orbit;
let selectedMesh = null;
let pickPosition = { x: 0, y: 0 };

export default class MainCanvas extends React.Component {
	canvasRef = React.createRef()

	componentDidMount() {
		meshServer.ui = this;
		this.world = new World();
		renderer = new THREE.WebGLRenderer({
			canvas: this.canvasRef.current,
		});
		renderer.setPixelRatio(window.devicePixelRatio)
		renderer.setSize(window.innerWidth, window.innerHeight)
		renderer.shadowMap.enabled = true
		// Orbit controls
		orbit = new OrbitControls(this.world.camera, renderer.domElement)
		orbit.update()
		orbit.addEventListener('change', this.render3D)
		control = new TransformControls(this.world.camera, renderer.domElement)
		control.addEventListener('change', this.meshControlChanged)
		control.addEventListener('dragging-changed', function (event) {
			//event.value true when starting drag. false when existing a drag.
			orbit.enabled = !event.value
			//only record position at end of drag.
			if(!event.value && selectedMesh) {
				hist.addMesh(selectedMesh)
			}
		});

		this.world.scene.add(control)
		
		this.bindEventListeners()
		this.render3D()
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
		const frustumHeight = this.world.camera.top - this.world.camera.bottom;
		this.world.camera.left = - frustumHeight * aspect / 2;
		this.world.camera.right = frustumHeight * aspect / 2;

		this.world.camera.updateProjectionMatrix();
		this.render3D();
	}

	setPickPosition = (event) => {
		pickPosition.x = (event.clientX / window.innerWidth) * 2 - 1
		pickPosition.y = - (event.clientY / window.innerHeight) * 2 + 1
	}

	selectObj = () => {
		const sM = pickHelper.pick(pickPosition, this.world.scene, 
			this.world.camera, this.world.meshes);
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
			this.world.scene.remove(selectedMesh)
			const i = selectedMesh.idx
			this.world.meshes.splice(i, 1)
			var j = i;
			for(; j<this.world.meshes.length; j++){
				this.world.meshes[j].idx --;
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
		let duplicate = new THREE.Mesh(mesh.geometry.clone(), mesh.material.clone())
		//remember to disable emission
		duplicate.material.emissive.setHex(0x0)
		duplicate.rotation.copy(mesh.rotation) // Copy rotation
		duplicate.position.copy(mesh.position) // Copy position
		duplicate.scale.copy(mesh.scale) // Copy position
		// Translate the mesh (in object space)
		duplicate.translateX(10)
		duplicate.translateZ(10)
	
		const i = selectedMesh.idx
		duplicate.name = this.world.meshes[i].name;		
		this.addMesh(duplicate)
	}

	addMesh = (mesh) => {
		if (mesh !== undefined) {
			this.world.scene.add(mesh);
			mesh.idx = this.world.meshes.length;
			this.world.meshes.push(mesh);
			control.attach(mesh);
			selectedMesh = mesh;
			hist.addMesh(mesh);
			this.props.showMeshTrans(selectedMesh);
		} else {
			alert("invalid mesh data.");
			return;
		}
		this.render3D();
	}

	render3D = () => {
		const aspect = window.innerWidth / window.innerHeight;
		this.world.camera.aspect = aspect;
		this.world.camera.updateProjectionMatrix();
		renderer.setSize(window.innerWidth, window.innerHeight);
		renderer.render(this.world.scene, this.world.camera);
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
		hist.addMesh(selectedMesh)
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
		hist.apply(selectedMesh, h)
		this.render3D();
	}

	render() {
		return (
			<canvas id="mainCanvas" ref={this.canvasRef}></canvas>
		)
	}

}

