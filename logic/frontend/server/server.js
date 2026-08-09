import { WebSocketServer } from 'ws';
import { createConnection } from 'net';
import { createServer } from 'http';

const WS_PORT = 3001;
const ENGINE_PORT = 3002;

let engineSocket = null;
let engineBuf = '';
let pendingCallbacks = [];

function connectEngine() {
  const sock = createConnection({ port: ENGINE_PORT }, () => {
    console.log(`[server] connected to engine on port ${ENGINE_PORT}`);
  });

  sock.on('data', (data) => {
    engineBuf += data.toString();
    const lines = engineBuf.split('\n');
    engineBuf = lines.pop();
    for (const line of lines) {
      try {
        const msg = JSON.parse(line);
        if (pendingCallbacks.length > 0) {
          pendingCallbacks.shift()(msg);
        }
      } catch { /* skip partial */ }
    }
  });

  sock.on('close', () => {
    console.log('[server] engine disconnected, retrying in 2s...');
    engineSocket = null;
    setTimeout(connectEngine, 2000);
  });

  sock.on('error', (err) => {
    console.error('[server] engine socket error:', err.message);
    sock.destroy();
  });

  engineSocket = sock;
}

function sendToEngine(msg) {
  return new Promise((resolve) => {
    pendingCallbacks.push(resolve);
    if (engineSocket) {
      engineSocket.write(JSON.stringify(msg) + '\n');
    } else {
      resolve({ error: 'engine not connected' });
    }
  });
}

connectEngine();

const httpServer = createServer((_req, res) => {
  res.writeHead(200, { 'Content-Type': 'text/plain' });
  res.end('logic-reasoner server ok\n');
});

const wss = new WebSocketServer({ server: httpServer });

wss.on('connection', (ws) => {
  console.log('[ws] client connected');

  ws.on('message', async (data) => {
    try {
      const msg = JSON.parse(data.toString());
      const reply = await sendToEngine(msg);
      ws.send(JSON.stringify(reply));
    } catch (err) {
      ws.send(JSON.stringify({ error: err.message }));
    }
  });

  ws.on('close', () => {
    console.log('[ws] client disconnected');
  });
});

httpServer.listen(WS_PORT, () => {
  console.log(`[server] listening on ws://localhost:${WS_PORT}`);
});
