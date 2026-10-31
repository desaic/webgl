// Runs the C++ engine (WebAssembly) off the UI thread.
import createEngine from "./engine/engine.mjs";

export interface SearchRequest {
  type: "search";
  id: number;
  fen: string; // position the game started from
  moves: string; // moves played since, coordinate notation, space separated
  maxDepth: number;
  timeMs: number;
  useBook: boolean;
}

// "how many of these moves are still opening book moves?" (answered at once, before any search)
export interface BookDepthRequest {
  type: "bookDepth";
  id: number;
  fen: string;
  moves: string;
}

export interface SearchReply {
  type: "search";
  id: number;
  best: string; // "e2e4", "e7e8q" or "none"
  score: number; // centipawns for the side to move
  depth: number;
  nodes: number;
  timedOut: boolean;
  ms: number;
  book: number; // 0 for a searched move, else how many source games played this book move
}

export interface BookDepthReply {
  type: "bookDepth";
  id: number;
  depth: number;
}

const ready = createEngine();

self.onmessage = async (e: MessageEvent<SearchRequest | BookDepthRequest | { type: "new" }>) => {
  const engine = await ready;
  const msg = e.data;
  const post = (reply: SearchReply | BookDepthReply) => (self as unknown as Worker).postMessage(reply);
  if (msg.type === "new") {
    engine.ccall("engine_new_game", null, [], []);
  } else if (msg.type === "bookDepth") {
    const depth: number = engine.ccall("engine_book_depth", "number", ["string", "string"], [msg.fen, msg.moves]);
    post({ type: "bookDepth", id: msg.id, depth });
  } else {
    const t0 = performance.now();
    const out: string = engine.ccall("engine_search", "string", ["string", "string", "number", "number", "number"],
      [msg.fen, msg.moves, msg.maxDepth, msg.timeMs, msg.useBook ? 1 : 0]);
    // "bestmove e2e4 score 15 depth 7 nodes 467968 timeout 1 book 0"
    const t = out.split(" ");
    post({
      type: "search",
      id: msg.id,
      best: t[1],
      score: +t[3],
      depth: +t[5],
      nodes: +t[7],
      timedOut: t[9] === "1",
      ms: Math.round(performance.now() - t0),
      book: +t[11],
    });
  }
};
