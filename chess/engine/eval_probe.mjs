// node engine/eval_probe.mjs : sanity probes of the static evaluation (needs build.sh first)
//   1. colour symmetry: a position and its colour-flipped mirror image must score the same
//   2. a table of hand-picked positions with their scores
//   3. can the engine win basic endgames (K+Q v K, K+R v K) against a bare king?
import createEngine from "../web/src/engine/engine.mjs";
import { Position, START_FEN, moveToUci } from "../web/src/rules.ts";

const m = await createEngine();
const evalFen = (fen) => m.ccall("engine_eval", "number", ["string"], [fen]);
const best = (fen, moves, ms) => m.ccall("engine_search", "string", ["string", "string", "number", "number", "number"], [fen, moves, 30, ms, 0]).split(" ")[1];

// the same position with the colours swapped (board flipped vertically, case swapped, side and rights swapped)
function mirror(fen) {
  const [placement, side, castling, ep] = fen.split(" ");
  const rows = placement.split("/").reverse().map((r) => [...r].map((c) => (c >= "a" && c <= "z" ? c.toUpperCase() : c >= "A" && c <= "Z" ? c.toLowerCase() : c)).join(""));
  const sw = [...(castling === "-" ? "" : castling)].map((c) => (c === c.toUpperCase() ? c.toLowerCase() : c.toUpperCase()));
  const order = "KQkq";
  const rights = sw.sort((a, b) => order.indexOf(a) - order.indexOf(b)).join("") || "-";
  const mep = ep === "-" ? "-" : ep[0] + (ep[1] === "3" ? "6" : "3");
  return `${rows.join("/")} ${side === "w" ? "b" : "w"} ${rights} ${mep} 0 1`;
}

// 1. symmetry over random game positions
let seed = 7;
const rnd = () => ((seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);
let checked = 0, asym = 0, worst = 0;
for (let g = 0; g < 40; g++) {
  const p = new Position(START_FEN);
  for (let ply = 0; ply < 60; ply++) {
    const moves = p.legalMoves();
    if (!moves.length || p.status() !== "playing") break;
    p.make(moves[Math.floor(rnd() * moves.length)]);
    const fen = p.getFen();
    const a = evalFen(fen), b = evalFen(mirror(fen));
    checked++;
    if (a !== b) { asym++; worst = Math.max(worst, Math.abs(a - b)); }
  }
}
console.log(`symmetry: ${asym} of ${checked} positions score differently when the colours are swapped (worst difference ${worst} cp)`);

// 2. hand-picked positions (score for the side to move, centipawns)
const table = [
  ["start position", START_FEN],
  ["white up a knight", "rnbqkb1r/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"],
  ["white up a rook", "1nbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"],
  ["K+R v K", "8/8/8/4k3/8/8/8/R3K3 w - - 0 1"],
  ["K+Q v K", "8/8/8/4k3/8/8/8/3QK3 w - - 0 1"],
  ["K+B v K (dead draw)", "8/8/8/4k3/8/8/8/B3K3 w - - 0 1"],
  ["K+N v K (dead draw)", "8/8/8/4k3/8/8/8/N3K3 w - - 0 1"],
  ["K+N v K+P", "8/8/8/4k3/8/8/p7/N3K3 w - - 0 1"],
  ["K+P v K, pawn on 2nd rank", "8/8/8/4k3/8/8/4P3/4K3 w - - 0 1"],
  ["K+P v K, pawn on 7th rank", "8/4P3/8/4k3/8/8/8/4K3 w - - 0 1"],
  ["king in the middle, queens on", "rnbq1bnr/pppp1ppp/8/4p3/4P3/8/PPPPKPPP/RNBQ1BNR w kq - 0 1"],
  ["after castling both sides (symmetric)", "r4rk1/pppppppp/8/8/8/8/PPPPPPPP/R4RK1 w - - 0 1"],
];
console.log("\nposition                               eval (side to move)");
for (const [name, fen] of table) console.log(name.padEnd(38), String(evalFen(fen)).padStart(6));

// 3. endgames against a bare king, both sides played by the engine (200 ms per move, no book)
function play(fen, maxPlies, ms) {
  const p = new Position(fen);
  const moves = [];
  for (let ply = 0; ply < maxPlies; ply++) {
    const st = p.status();
    if (st !== "playing") return { result: st, plies: ply };
    const u = best(fen, moves.join(" "), ms);
    const mv = p.legalMoves().find((x) => moveToUci(x) === u);
    if (!mv) return { result: "no move", plies: ply };
    moves.push(u);
    p.make(mv);
  }
  return { result: "not finished", plies: maxPlies };
}
console.log("\nendgames (engine plays both sides, 150 ms per move, 100 plies allowed):");
for (const [name, fen] of [["K+Q v K", "8/8/8/4k3/8/8/8/3QK3 w - - 0 1"], ["K+R v K", "8/8/8/4k3/8/8/8/R3K3 w - - 0 1"], ["K+Q v K, far apart", "7k/8/8/8/8/8/8/KQ6 w - - 0 1"]]) {
  const r = play(fen, 100, 150);
  console.log(`${name.padEnd(22)} -> ${r.result} after ${r.plies} plies`);
}
