import React from 'react'
import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls'
import { TransformControls } from 'three/examples/jsm/controls/TransformControls.js'
import PickHelper from './PickHelper'
import './MainCanvas.scss'
import { MeshStateHistory } from '../utils/MeshStateHistory'
import World from './World'
import Emu from './Emu'

const world = new World();
const meshHistory = new MeshStateHistory();
const pickHelper = new PickHelper();
const webSocket = new WebSocket('ws://localhost:9000/')

let renderer;
let control;
let orbit;
let selectedMesh = null;
let pickPosition = { x: 0, y: 0 };
let exeData = null;
const emu = new Emu();

var simIntervalId =0;
var animateIntervalId;

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
		orbit.addEventListener('change', this.updateCanvasRender)
		control = new TransformControls(world.camera, renderer.domElement)
		control.addEventListener('change', this.updateCanvasRender)
		control.addEventListener('dragging-changed', function (event) {
			orbit.enabled = !event.value
			if(event.value && selectedMesh) {
				meshHistory.setTransientMeshState(selectedMesh)
			}
			if(!event.value && selectedMesh) {
				meshHistory.recordMeshStateChange(selectedMesh)
			}
		});

		world.scene.add(control)
		
		this.bindEventListeners()
		this.clearPickPosition()
		this.updateCanvasRender()
		
		webSocket.onmessage = (message) => {
			this.parseSocketMsg(message.data);
		};
		animateIntervalId = setInterval(this.animate, 16);
		simIntervalId = setInterval(this.OneStep, 16);
	}

	shouldComponentUpdate() {
		return false
	}

	bindEventListeners = () => {
		const canvasElement = this.canvasRef.current
		window.addEventListener('resize', this.updateCanvasRender, false)
		canvasElement.addEventListener('click', this.selectObj)
		//canvasElement.addEventListener('mousemove', this.setPickPosition)
		//canvasElement.addEventListener('mouseout', this.clearPickPosition)
		//canvasElement.addEventListener('mouseleave', this.clearPickPosition)

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
		const sM = pickHelper.pick(pickPosition, world.scene, world.camera, world.meshes);
		if (sM) {
			selectedMesh = sM
			control.attach(selectedMesh)
		}
		//this.updateCanvasRender()
	}

	setMode = (mode) => {
		control.setMode(mode)
	}

	deleteSelected = () => {
		if (selectedMesh) {
			// Remove the object from scene
			meshHistory.clearHistory(selectedMesh.uuid)
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
		this.updateCanvasRender()
	}
	
	deselectMesh = () => {
		if(selectedMesh) {
			control.detach()
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
		} else {
			alert("invalid mesh data.");
			return;
		}
		this.updateCanvasRender();
	}

	animate = ()  => {
		this.updateCanvasRender();
	}

	updateCanvasRender = () => {
		const aspect = window.innerWidth / window.innerHeight;
		world.camera.aspect = aspect;
		world.camera.updateProjectionMatrix();

		renderer.setSize(window.innerWidth, window.innerHeight);
		//PICK_HELPER.pick(pickPosition, SCENE, CURRENT_CAMERA,meshes)
		renderer.render(world.scene, world.camera);
		this.props.onSelectedMeshDataChange(selectedMesh);
	}

	handlePositionXChange = (event) => {
		meshHistory.setTransientMeshState(selectedMesh)
		selectedMesh.position.x = event.target.value
		meshHistory.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}

	handlePositionYChange = (event) => {
		meshHistory.setTransientMeshState(selectedMesh)
		selectedMesh.position.y = event.target.value
		meshHistory.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}
	handlePositionZChange = (event) => {
		meshHistory.setTransientMeshState(selectedMesh)
		selectedMesh.position.z = event.target.value
		meshHistory.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}

	handleRoatationXChange = (event) => {
		meshHistory.setTransientMeshState(selectedMesh)
		selectedMesh.rotation.x = event.target.value
		meshHistory.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}
	handleRoatationYChange = (event) => {
		meshHistory.setTransientMeshState(selectedMesh)
		selectedMesh.rotation.y = event.target.value
		meshHistory.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}
	handleRoatationZChange = (event) => {
		meshHistory.setTransientMeshState(selectedMesh)
		selectedMesh.rotation.z = event.target.value
		meshHistory.recordMeshStateChange(selectedMesh)
		this.updateCanvasRender()
	}

	handleUndoAction = () => {
		const meshStateChange = meshHistory.popHistory()
		if(!meshStateChange) {
			return
		}
		world.meshes.forEach(mesh => {
			if(meshStateChange.id === mesh.uuid) {
				meshHistory.applyStateToMesh(mesh, meshStateChange.previousState)
			}
		})
		this.updateCanvasRender()
	}

	OneStep = () =>{
		emu.regs[3] ++;
		this.props.onVarListChange([...emu.regs]);
	}

	rgbToHex = (rgb) =>{
		var hex = Number(rgb).toString(16);
		if (hex.length < 2) {
			 hex = "0" + hex;
		}
		return "#"+hex;
	}

	DrawPPU = (chrrom) =>{
		var canvas = world.canvas;
		var context = canvas.getContext('2d');
		context.fillStyle='black';
		context.fillRect(0,0, canvas.width,canvas.height);
		//const rows = 480;
		//const cols = 256;
		const cols = 16;
		const numTiles = 256;
		var t;
		var tileoffset = 0;
		var byteOffset = 0;

		var id = context.createImageData(4,4); // only do this once per page
		var d  = id.data;

		for(let p = 0; p<d.length; p+=4){
			d[p+3]   = 255;
		}
		
		for(t = tileoffset; t<numTiles; t++){
			var tileRow = Math.floor( (t-tileoffset) /cols);
			var tileCol = (t-tileoffset) % cols;
			var pRow, pCol;
			var byteStart = t * 16 + byteOffset;
			for(pRow = 0; pRow<8; pRow++){
				var b = byteStart + pRow ;
				var byte1 = chrrom[b];
				var byte2 = chrrom[b+8];
				for(pCol = 0; pCol<8; pCol++){
					var bit1 = (byte1>>(7-pCol)) & 1;
					var bit2 = (byte2>>(7-pCol)) & 1;
					var sum = bit1 + 2*bit2;
					sum = 80*sum;
					
					for(let p = 0; p<d.length; p+=4){
						d[p]   = sum;
						d[p+1]   = sum;
						d[p+2]   = sum/1.5;	
					}
					//context.fillStyle=hex;
					//context.fillRect (40*tileCol + pCol*5, 40*tileRow+pRow*5, 5, 5 );
					context.putImageData( id, 4*(9*tileCol + pCol), 4*(9*tileRow+pRow) );
				}
			}
		}

		world.screen.material.map.needsUpdate = true;
	}

	ReadBinFile = (data) =>{
		exeData = data;
		var s= emu.disas(exeData);
		this.DrawPPU(data);
		console.log(s);
	}

	OpenFile = (event) =>{
		var files = event.target.files;
		if(files.length==0){
			return;
		}
		var fileData = new Blob([files[0]]);
		var promise = new Promise(this.getBuffer(fileData));
		promise.then(this.ReadBinFile).catch(function(err) {
			console.log('Error: ',err);
		  });
	}

	getBuffer = (fileData) =>{
		return function(resolve) {
		  var reader = new FileReader();
		  reader.readAsArrayBuffer(fileData);
		  reader.onload = function() {
			var arrayBuffer = reader.result
			var bytes = new Uint8Array(arrayBuffer);
			resolve(bytes);
		  }
	  }
	}

	render() {
		return (
			<div id="fileDiv" className="">
			<div id="fileDiv" className="FileInput">
				<input type="file" id="Upload" name="fileUpload"
				onChange={this.OpenFile}/>
				</div>
				<canvas id="mainCanvas" ref={this.canvasRef}></canvas>
			</div>
			
		)
	}
}
