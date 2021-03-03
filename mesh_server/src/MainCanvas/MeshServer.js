import * as THREE from 'three'

const CommandName = {"MESH":1, "TRIGS": 2,
"ATTR":3, "TEXTURE":4, "TRANS":5, "GET":6} 

const SceneMember = {"NUM_MESHES":1}

const MeshEvents = {"NUM_MESHES":1}

export default class MeshServer
{
    constructor(ui) {
        this.webSocket = new WebSocket('ws://localhost:9000/')
        this.webSocket.onmessage = (message) => {
			this.parseSocketMsg(message.data);
		};
        this.ui = ui;
        this.littleEndian = true;
    }

    parseMeshCmd = (buf) =>{
        //|msg type 2 bytes| meshid 2 bytes | numTrigs 4 bytes
        //does not support more than 4 billion trigs.
        const HEADER_BYTES = 8;
        const header = new Uint16Array(buf,0,4);
        if(buf.length<HEADER_BYTES){
            console.log("message too small");
            return;
        }
        const meshId = header[1];
        ///\TODO bad hack. not sure best way to do this.
        const numTrigs = header[2] + 65536*header[3];
        const fvals = new Float32Array(buf,HEADER_BYTES);
        this.createMesh(meshId, fvals);
    }

    parseGetCmd = (buf)=>{
        const header = new Uint16Array(buf,0,2);
        const member = header[1];
        if(member == SceneMember.NUM_MESHES){
            const numMeshes = this.ui.world.meshes.length
            const buffer = new ArrayBuffer(6);
            //|NUM_MESHES 2 bytes | 4 bytes of number of meshes
            var view = new DataView(buffer);
            
            view.setUint16(0, MeshEvents.NUM_MESHES, this.littleEndian);
            view.setUint32(2, numMeshes, this.littleEndian);
            this.webSocket.send(buffer);
        }
    }

	parseBlob = (blob) => {
		new Response(blob).arrayBuffer()
		.then(buf => {
            //|msg type 2 bytes| meshid 2 bytes | numTrigs 4 bytes
            //does not support more than 4 billion trigs.
            const header = new Uint16Array(buf,0,2);
			const cmd = header[0];
            if(cmd == CommandName.MESH){
                this.parseMeshCmd(buf);
            }else if(cmd == CommandName.GET){
                this.parseGetCmd(buf);
            }
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
        mesh.name  =meshId.toString();
		this.ui.addMesh(mesh, meshId);
	}

}
