// node engine/test_search.mjs : sanity checks for the timed search (needs build.sh first)
import createEngine from "../web/src/engine/engine.mjs";
const m = await createEngine();
const search = (fen, moves, depth, ms) => {
  const t = performance.now();
  const r = m.ccall("engine_search", "string", ["string", "string", "number", "number"], [fen, moves, depth, ms]);
  return r + `  (${Math.round(performance.now() - t)} ms)`;
};
const start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
console.log("mate in 1 (expect h5f7 or similar)\n ", search("r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", "", 6, 0));
console.log("mate in 2\n ", search("r2qkb1r/pp2nppp/3p4/2pNN1B1/2BnP3/3P4/PPP2PPP/R2bK2R w KQkq - 1 1", "", 6, 0));
console.log("back-rank mate in 1 (a1a8)\n ", search("6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1", "", 4, 0));
for (const ms of [50, 200, 1000]) console.log(`startpos, ${ms} ms budget, depth cap 30\n `, search(start, "", 30, ms));
console.log("stalemate-avoid / no moves\n ", search("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1", "", 4, 0));
console.log("legal moves", m.ccall("engine_legal_moves", "string", ["string"], [start]).trim().split(" ").length);
