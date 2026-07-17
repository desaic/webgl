import { GraphRenderer } from './graph-renderer';
import { ChatPanel } from './chat';
import type { ProofGraph } from './types';

const upload = document.getElementById('upload') as HTMLInputElement;
const status = document.getElementById('status') as HTMLDivElement;

const renderer = new GraphRenderer(document.body);
const chat = new ChatPanel();

upload.addEventListener('change', async () => {
  const file = upload.files?.[0];
  if (!file) return;

  status.textContent = `Loading ${file.name}...`;
  try {
    const text = await file.text();
    const graph: ProofGraph = JSON.parse(text);
    renderer.loadGraph(graph);
    renderer.resetCamera();
    const nodeCount = graph.nodes.length;
    const edgeCount = graph.edges.length;
    status.textContent = `${nodeCount} nodes, ${edgeCount} edges`;
  } catch (err) {
    status.textContent = `Error: ${(err as Error).message}`;
  }
});
