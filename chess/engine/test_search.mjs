// node engine/test_search.mjs : sanity checks for the timed search (needs build.sh first)
import createEngine from "../web/src/engine/engine.mjs";
const m = await createEngine();
const search = (fen, moves, depth, ms, book = 0) => {
  const t = performance.now();
  const r = m.ccall("engine_search", "string", ["string", "string", "number", "number", "number"], [fen, moves, depth, ms, book]);
  return r + `  (${Math.round(performance.now() - t)} ms)`;
};
const start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
console.log("mate in 1 (expect h5f7 or similar)\n ", search("r1bqkbnr/pppp1ppp/2n5/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", "", 6, 0));
console.log("mate in 2\n ", search("r2qkb1r/pp2nppp/3p4/2pNN1B1/2BnP3/3P4/PPP2PPP/R2bK2R w KQkq - 1 1", "", 6, 0));
console.log("back-rank mate in 1 (a1a8)\n ", search("6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1", "", 4, 0));
for (const ms of [50, 200, 1000]) console.log(`startpos, ${ms} ms budget, depth cap 30\n `, search(start, "", 30, ms));
console.log("stalemate-avoid / no moves\n ", search("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1", "", 4, 0));
console.log("legal moves", m.ccall("engine_legal_moves", "string", ["string"], [start]).trim().split(" ").length);

// opening book
const bookMove = (moves, book = 1) => search(start, moves, 6, 200, book).split(" ");
let failed = 0;
const check = (ok, msg) => { if (!ok) failed++; console.log((ok ? "ok    " : "FAIL  ") + msg); };

const first = bookMove("");
check(+first[11] > 0, "startpos is in the book: " + first[1] + " (played " + first[11] + " times in the source games)");
const seen = new Map();
for (let i = 0; i < 60; i++) { const mv = bookMove("")[1]; seen.set(mv, (seen.get(mv) ?? 0) + 1); }
check(seen.size >= 2, "book picks vary between games: " + [...seen].map(([k, v]) => k + "x" + v).join(" "));
const legalFirst = m.ccall("engine_legal_moves", "string", ["string"], [start]).trim().split(" ");
check([...seen.keys()].every((k) => legalFirst.includes(k)), "every book move at the start is legal");
const search1 = bookMove("", 0);
check(+search1[11] === 0 && +search1[5] > 0, "book switched off: searched instead (depth " + search1[5] + ")");
const reply = bookMove("e2e4 e7e5 g1f3");
check(+reply[11] > 0, "1.e4 e5 2.Nf3 is in the book: reply " + reply[1]);
const late = bookMove("e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 c2c3 e8g8 h2h3 c6a5 b3c2 c7c5 d2d4 d8c7 b1d2 a5c6");
check(+late[11] === 0 && +late[5] > 0, "deep in a game the engine searches instead (depth " + late[5] + ")");
const depthOf = (moves) => m.ccall("engine_book_depth", "number", ["string", "string"], [start, moves]);
check(depthOf("") === 0, "book depth of an empty game is 0");
check(depthOf("e2e4 e7e5 g1f3 b8c6 f1b5") >= 5, "Ruy Lopez moves are all book moves: depth " + depthOf("e2e4 e7e5 g1f3 b8c6 f1b5"));
check(depthOf("a2a3 a7a6 h2h3") <= 1 || depthOf("a2a3 a7a6 h2h3") < 3, "an odd line leaves the book early: depth " + depthOf("a2a3 a7a6 h2h3"));
check(depthOf("e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 c2c3 e8g8") <= 16, "book depth never exceeds the book's ply limit");
process.exit(failed ? 1 : 0);
