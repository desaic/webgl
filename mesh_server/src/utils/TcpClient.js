exports.TcpClient = class TcpClient {
    constructor(){
        this.sock = null;
        //if no header, expect the first thing received to be a header.
	    this.hasHeader = false;
	    this.receivedBytes = 0;
    	//total number of bytes for payload according to header.
	    this.payloadSize = 0;
    	//any left over bytes from previous message.
        this.recvBuf="";
        this.completeMsg = null;
        this.msgBuf = null
        this.msgCallback = null;
    }

    recv(data) {
        if(!this.hasHeader) {
            this.recvBuf += data.toString('latin1');
            console.log(this.recvBuf.length + " " + data.length);
            var startIdx = this.recvBuf.indexOf("message_size");
            //if no proper header found discard other data.
            if(startIdx<0) {
                recvBuf = "";
                return;
            }
            var endIdx = this.recvBuf.indexOf("\r\n");
            if(endIdx<0 || endIdx>startIdx + 40) {
                this.recvBuf = "";
                console.log("invalid header. expect \\r\\n.");
                return;
            }
            var header = this.recvBuf.slice(startIdx, endIdx);
            var tokens = header.split(" ");
            this.payloadSize = parseInt(tokens[1]);
            console.log("expect image size " + this.payloadSize);
            this.hasHeader = true;
            data = Buffer.from(this.recvBuf.slice(endIdx + 2), 'latin1');
            this.receivedBytes = 0;
            this.msgBuffer = Buffer.allocUnsafe(this.payloadSize);
            this.recvBuf = "";
        }

        if(this.hasHeader) {
            endIdx = data.length;
            //we are reading into the next message
            //or just got extra incorrect data.
            var endOfMsg = false;
            if(this.receivedBytes + endIdx>=this.payloadSize) {
                endIdx = this.payloadSize - this.receivedBytes;
                endOfMsg = true;
                this.hasHeader = false;
            }
            data.copy(this.msgBuffer, this.receivedBytes, 0, endIdx);
            this.receivedBytes += endIdx;
            if(endOfMsg) {
                this.completeMsg = this.msgBuffer;
                this.recvBuf = data.slice(endIdx).toString("latin1");
                console.log("msg complete with size: " + this.receivedBytes);
                console.log("extra bytes " + (this.receivedBytes - this.payloadSize));
                this.receivedBytes = 0;
                if(this.msgCallback != null ){
                    this.msgCallback(this.completeMsg);
                }
            }
        }
    }
}