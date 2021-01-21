const http = require('http');
const url = require('url');
const fs = require('fs');
const path = require('path');
var net = require('net');
var WebSocketServer = require('websocket').server;
const TcpClient = require('./src/utils/TcpClient.js').TcpClient;
const port = process.argv[2] || 9000;
//port for print client to connect to
const printPort = process.argv[3] || 9001

//tcp server ---
var server = net.createServer();
//emitted when server closes ...not emitted until all connections closes.
server.on('close',function(){
  console.log('Server closed !');
});

var tcpClient = new TcpClient();
tcpClient.msgCallback = processMsg;
// emitted when new client connects ----------------------------------------------------
server.on('connection', function(socket) {
	if(tcpClient.sock != null){
		console.log('refuse connection, a tcp client is already connected');
		socket.close();
		return;
	}else{
		tcpClient.sock = socket;
	}
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
	server.getConnections(function(error,count) {
		console.log('Number of concurrent connections to the server : ' + count);
	});

	socket.setTimeout(800000, function() {
		console.log('Socket timed out');
	});

	socket.on('data', function(data){
		tcpClient.recv(data);
	});

	socket.on('error',function(error){
	  console.log('TCP server Error : ' + error);
	});

	socket.on('timeout',function(){
	  console.log('Socket timed out !');
	  socket.end('Timed out!');
	  // can call socket.destroy() here too.
	  socket.destroy();
	  tcpClient.sock = null;
	});

	socket.on('end', function(data){
	  var bread = socket.bytesRead;
	  console.log('socket end Bytes read : ' + bread);
	  socket.destroy();
	  tcpClient.sock = null;
	});

	socket.on('close', function(error){
	  var bread = socket.bytesRead;
	  var bwrite = socket.bytesWritten;
	  console.log('Bytes read : ' + bread);
	  console.log('Bytes written : ' + bwrite);
	  console.log('Socket closed!');
	  if(error){
		console.log('Socket was closed coz of transmission error');
	  }
	  socket.destroy();
	  tcpClient.sock = null;
	});

	setTimeout(function(){
	  var isdestroyed = socket.destroyed;
	  console.log('Socket destroyed:' + isdestroyed);
	  socket.destroy();
	  tcpClient.sock = null;
	},1200000);

});

//----------------------------------------------------------------------------------------------------
// emits when any error occurs -> calls closed event immediately after this.
server.on('error',function(error){
  console.log('Error: ' + error);
});

//----------------------------------------------------------------------------------------------------
//emits when server is bound with server.listen
server.on('listening',function(){
  console.log('Server is listening!');
});
//----------------------------------------------------------------------------------------------------
server.maxConnections = 10;
//----------------------------------------------------------------------------------------------------
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

//----------------------------------------------------------------------------------------------------
// special in memory directory containing machine state
const stateDir = 'state';
var layerCount = 0;

var hServer = http.createServer(function (req, res) {
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

//--- web socket server
wsServer = new WebSocketServer({
	httpServer: hServer
});

let wsconnection;
wsServer.on('request', function (request) {
	wsconnection = request.accept(null, request.origin);

	wsconnection.on('message', function (message) {
		console.log('Received Message: ', message.utf8Data);
        wsconnection.send("image complete\r\n");
	});

	wsconnection.on('close', function (reasonCode, description) {
		console.log('client disconnect');
		wsconnection = null;
	});
});

function processMsg(msg)
{
  console.log("buffer size " + msg.length);
  if(wsconnection != undefined){
	  wsconnection.send(msg);
  }
}