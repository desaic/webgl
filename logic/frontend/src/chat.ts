export interface ChatMessage {
  role: 'user' | 'engine';
  text: string;
  timestamp: number;
}

export class ChatPanel {
  private container: HTMLDivElement;
  private messages: HTMLDivElement;
  private input: HTMLInputElement;
  private ws: WebSocket | null = null;
  private onMessage: ((msg: ChatMessage) => void) | null = null;

  constructor() {
    this.container = document.createElement('div');
    this.container.id = 'chat-panel';
    this.container.innerHTML = `
      <div id="chat-header">
        <span>Chat</span>
        <button id="chat-toggle" title="Toggle chat">─</button>
      </div>
      <div id="chat-messages"></div>
      <div id="chat-input-row">
        <input id="chat-input" type="text" placeholder="Ask or command..." />
        <button id="chat-send">Send</button>
      </div>
    `;
    document.body.appendChild(this.container);

    this.messages = this.container.querySelector('#chat-messages')!;
    this.input = this.container.querySelector('#chat-input')!;

    this.container.querySelector('#chat-send')!.addEventListener('click', () => this.send());
    this.input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') this.send();
    });

    this.container.querySelector('#chat-toggle')!.addEventListener('click', () => {
      this.container.classList.toggle('collapsed');
    });

    this.connect();
  }

  setMessageHandler(fn: (msg: ChatMessage) => void) {
    this.onMessage = fn;
  }

  private connect() {
    try {
      this.ws = new WebSocket('ws://localhost:3001');
      this.ws.onopen = () => this.addMessage('engine', 'Connected. Ready.');
      this.ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          this.addMessage('engine', typeof data === 'string' ? data : JSON.stringify(data, null, 2));
        } catch {
          this.addMessage('engine', event.data);
        }
      };
      this.ws.onclose = () => {
        this.addMessage('engine', 'Disconnected. Retrying...');
        setTimeout(() => this.connect(), 2000);
      };
    } catch {
      this.addMessage('engine', 'Cannot connect to server.');
    }
  }

  private send() {
    const text = this.input.value.trim();
    if (!text || !this.ws || this.ws.readyState !== WebSocket.OPEN) return;
    this.input.value = '';
    this.addMessage('user', text);
    this.ws.send(text);
  }

  addMessage(role: 'user' | 'engine', text: string) {
    const msg: ChatMessage = { role, text, timestamp: Date.now() };
    const el = document.createElement('div');
    el.className = `chat-msg chat-${role}`;
    el.textContent = text;
    this.messages.appendChild(el);
    this.messages.scrollTop = this.messages.scrollHeight;
    this.onMessage?.(msg);
  }
}
