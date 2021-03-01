import * as THREE from 'three'
export default class MeshServer
{
    constructor(ui) {
        this.webSocket = new WebSocket('ws://localhost:9000/')
        this.webSocket.onmessage = (message) => {
			this.parseSocketMsg(message.data);
		};
        this.ui = ui;
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

    
	createMesh = (meshId, trigs)=>{
		//make a debug mesh.
		const geometry = new THREE.BufferGeometry();
		geometry.setAttribute( 'position', new THREE.BufferAttribute( trigs, 3 ) );
		//geometry.computeFaceNormals();
		geometry.computeVertexNormals();
		const material = new THREE.MeshPhongMaterial( { color: 0x999999, specular: 0x101010 } );
		const mesh = new THREE.Mesh( geometry, material );
		this.ui.addMesh(meshId.toString(), mesh);
	}

}
