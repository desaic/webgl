// Runs the C++ engine (WebAssembly) off the UI thread.
import createEngine from "./engine/engine.mjs";

export interface SearchRequest {
  id: number;
  fen: string; // position the game started from
  moves: string; // moves played since, coordinate notation, space separated
  maxDepth: number;
  timeMs: number;
}

export interface SearchReply {
  id: number;
  best: string; // "e2e4", "e7e8q" or "none"
  score: number; // centipawns for the side to move
  depth: number;
  nodes: number;
  timedOut: boolean;
  ms: number;
}

const ready = createEngine();

self.onmessage = async (e: MessageEvent<SearchRequest | { type: "new" }>) => {
  const engine = await ready;
  const msg = e.data as any;
  if (msg.type === "new") {
    engine.ccall("engine_new_game", null, [], []);
    return;
  }
  const req = msg as SearchRequest;
  const t0 = performance.now();
  const out: string = engine.ccall("engine_search", "string", ["string", "string", "number", "number"],
    [req.fen, req.moves, req.maxDepth, req.timeMs]);
  // "bestmove e2e4 score 15 depth 7 nodes 467968 timeout 1"
  const t = out.split(" ");
  const reply: SearchReply = {
    id: req.id,
    best: t[1],
    score: +t[3],
    depth: +t[5],
    nodes: +t[7],
    timedOut: t[9] === "1",
    ms: Math.round(performance.now() - t0),
  };
  (self as unknown as Worker).postMessage(reply);
};
