const http = require('http');
const url = require('url');
const fs = require('fs');
const path = require('path');
var net = require('net');

const port = process.argv[2] || 9000;
//port for print client to connect to 
const printPort = process.argv[3] || 9001

//tcp server ---
var server = net.createServer();
//emitted when server closes ...not emitted until all connections closes.
server.on('close',function(){
  console.log('Server closed !');
});

var imgBuffer;
var completeImage = null;
// emitted when new client connects
server.on('connection',function(socket){
//this property shows the number of characters currently buffered to be written. (Number of characters is approximately equal to the number of bytes to be written, but the buffer may contain strings, and the strings are lazily encoded, so the exact number of bytes is not known.)

	console.log('Buffer size : ' + socket.bufferSize);
	console.log('---------server details -----------------');
	var address = server.address();
	var port = address.port;
	var family = address.family;
	var ipaddr = address.address;
	console.log('Server is listening at port ' + port);
	console.log('Server ip : ' + ipaddr);
	console.log('Server is IP4/IP6 : ' + family);

	var lport = socket.localPort;
	var laddr = socket.localAddress;
	console.log('Server is listening at LOCAL port ' + lport);
	console.log('Server LOCAL ip :' + laddr);

	console.log('------------remote client info --------------');
	var rport = socket.remotePort;
	var raddr = socket.remoteAddress;
	var rfamily = socket.remoteFamily;
	console.log('REMOTE Socket is listening at port ' + rport);
	console.log('REMOTE Socket ip : ' + raddr);
	console.log('REMOTE Socket is IP4/IP6 : ' + rfamily);
	console.log('--------------------------------------------')
	server.getConnections(function(error,count){
	console.log('Number of concurrent connections to the server : ' + count);
	});

	socket.setTimeout(800000,function(){
	// called after timeout -> same as socket.on('timeout')
	// it just tells that soket timed out => its ur job to end or destroy the socket.
	// socket.end() vs socket.destroy() => end allows us to send final data and allows some i/o activity to finish before destroying the socket
	// whereas destroy kills the socket immediately irrespective of whether any i/o operation is goin on or not...force destry takes place
	console.log('Socket timed out');
	});

	//if no header, expect the first thing received to be a header.
	var hasHeader = false;
	var bufferIndex = 0;
	//total number of bytes for image data
	var imgSize = 0;
	//any left over bytes from previous image.
	var recvBuf="";
	socket.on('data',function(data){
		if(!hasHeader){
			recvBuf += data.toString('latin1');
			console.log(recvBuf.length + " " + data.length);
			var startIdx = recvBuf.indexOf("image_buffer_size");
			//if no proper header found discard other data.
			if(startIdx<0){
				recvBuf = "";
				return;
			}
			var endIdx = recvBuf.indexOf("\r\n");
			if(endIdx<0 || endIdx>startIdx + 40){
				recvBuf = "";
				console.log("invalid header. expect \\r\\n.");
				return;
			}
			header = recvBuf.slice(startIdx, endIdx);
			tokens = header.split(" ");
			imgSize = parseInt(tokens[1]);
			console.log("expect image size " + imgSize);
			hasHeader = true;
			data = Buffer.from(recvBuf.slice(endIdx + 2), 'latin1');
			bufferIndex = 0;
			imgBuffer = Buffer.allocUnsafe(imgSize);
			recvBuf = "";
		}		
		
		if(hasHeader){
			endIdx = data.length;
			//we are reading into the next image 
			//or just got extra incorrect data.
			var endOfImg = false;
			if(bufferIndex + endIdx>=imgSize){
				endIdx = imgSize - bufferIndex;
				endOfImg = true;
				hasHeader = false;
			}
			data.copy(imgBuffer, bufferIndex, 0, endIdx);
			bufferIndex += endIdx;
			if(endOfImg){
				completeImage = imgBuffer;
				recvBuf = data.slice(endIdx).toString("latin1");
				console.log("img complete " + bufferIndex);
				console.log("extra bytes " + (bufferIndex - imgSize));
				bufferIndex = 0;
			}
		}
	});

	socket.on('error',function(error){
	  console.log('TCP server Error : ' + error);
	});

	socket.on('timeout',function(){
	  console.log('Socket timed out !');
	  socket.end('Timed out!');
	  // can call socket.destroy() here too.
	});

	socket.on('end',function(data){
	  var bread = socket.bytesRead;
	  console.log('socket end Bytes read : ' + bread);
	});

	socket.on('close',function(error){
	  var bread = socket.bytesRead;
	  var bwrite = socket.bytesWritten;
	  console.log('Bytes read : ' + bread);
	  console.log('Bytes written : ' + bwrite);
	  console.log('Socket closed!');
	  if(error){
		console.log('Socket was closed coz of transmission error');
	  }
	}); 

	setTimeout(function(){
	  var isdestroyed = socket.destroyed;
	  console.log('Socket destroyed:' + isdestroyed);
	  socket.destroy();
	},1200000);

});

// emits when any error occurs -> calls closed event immediately after this.
server.on('error',function(error){
  console.log('Error: ' + error);
});

//emits when server is bound with server.listen
server.on('listening',function(){
  console.log('Server is listening!');
});

server.maxConnections = 10;

//static port allocation
server.listen(printPort);

var islistening = server.listening;

if(islistening){
  console.log('TCP Server is listening');
}else{
  console.log('TCP Server is not listening');
}

setTimeout(function(){
  server.close();
},5000000);

//http server ---
// maps file extention to MIME types
const mimeType = {
  '.ico': 'image/x-icon',
  '.html': 'text/html',
  '.js': 'text/javascript',
  '.json': 'application/json',
  '.css': 'text/css',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.wav': 'audio/wav',
  '.mp3': 'audio/mpeg',
  '.svg': 'image/svg+xml',
  '.pdf': 'application/pdf',
  '.doc': 'application/msword',
  '.eot': 'appliaction/vnd.ms-fontobject',
  '.ttf': 'aplication/font-sfnt'
};

// special in memory directory containing machine state
const stateDir = 'state';
var layerCount = 0;

http.createServer(function (req, res) {
  console.log(`${req.method} ${req.url}`);

  // parse URL
  const parsedUrl = url.parse(req.url);

  // extract URL path
  // Avoid https://en.wikipedia.org/wiki/Directory_traversal_attack
  // e.g curl --path-as-is http://localhost:9000/../fileInDanger.txt
  // by limiting the path to current directory only
  const sanitizePath = path.normalize(parsedUrl.pathname).replace(/^(\.\.[\/\\])+/, '');
  let pathname = path.join(__dirname, sanitizePath);
  
  //get root dir of the url and check if it's in memory on on disk.
  var firstDir = sanitizePath;
  firstDir.toLowerCase();
  var tokens = firstDir.split("\\");
  console.log(tokens[1]);
  if(tokens[1] === stateDir){
	  if(tokens.length<3){
        res.statusCode = 404;
		res.end('need a variable name under /state/');
	  }
	  var varName = tokens[2];
	  if(varName === "layer"){
		res.setHeader('Content-type', 'text/plain' );
		res.end(layerCount.toString());
		layerCount = layerCount + 1;
		return;
	  }else if(varName === "scan.png"){
		if(completeImage == null){
			res.setHeader('Content-type', 'text/plain' );
			res.end("scan not available");
			return;
		}
		res.setHeader('Content-type', 'image/png' );
		res.end(completeImage);
		return;
	  }
  }
  
  fs.exists(pathname, function (exist) {
    if(!exist) {
      // if the file is not found, return 404
      res.statusCode = 404;
      res.end(`File ${pathname} not found!`);
      return;
    }

    // if is a directory, then look for index.html
    if (fs.statSync(pathname).isDirectory()) {
      pathname += '/index.html';
    }

    // read file from file system
    fs.readFile(pathname, function(err, data){
      if(err){
        res.statusCode = 500;
        res.end(`Error getting the file: ${err}.`);
      } else {
        // based on the URL path, extract the file extention. e.g. .js, .doc, ...
        const ext = path.parse(pathname).ext;
        // if the file is found, set Content-type and send data
        res.setHeader('Content-type', mimeType[ext] || 'text/plain' );
        res.end(data);
      }
    });
  });
}).listen(parseInt(port));

console.log(`Server listening on port ${port}`);