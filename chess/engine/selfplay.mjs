// node engine/selfplay.mjs [msPerMove] [baseline.mjs] : new engine v a saved baseline engine.
// Each opening is played twice with colours swapped. Prints the new engine's W/D/L and the score.
import createNew from "../web/src/engine/engine.mjs";
import { Position, START_FEN, moveToUci, WHITE } from "../web/src/rules.ts";

const ms = +(process.argv[2] ?? 100);
const basePath = process.argv[3] ?? "../ignore/engine_baseline.mjs";
const base = await (await import(basePath)).default();
const next = await createNew();

const OPENINGS = [
  "e2e4 e7e5 g1f3 b8c6", "e2e4 c7c5 g1f3 d7d6", "e2e4 e7e6 d2d4 d7d5", "d2d4 d7d5 c2c4 e7e6",
  "d2d4 g8f6 c2c4 g7g6", "g1f3 d7d5 g2g3 g8f6", "c2c4 e7e5 b1c3 g8f6", "e2e4 c7c6 d2d4 d7d5",
  "d2d4 g8f6 c2c4 e7e6", "e2e4 d7d6 d2d4 g8f6", "e2e4 e7e5 g1f3 g8f6", "d2d4 d7d5 g1f3 g8f6",
  "c2c4 c7c5 g1f3 g8f6", "e2e4 g7g6 d2d4 f8g7", "d2d4 f7f5 g2g3 g8f6", "e2e4 b8c6 d2d4 d7d5",
];

const search = (m, fen, moves) =>
  m.ccall("engine_search", "string", ["string", "string", "number", "number", "number"], [fen, moves.join(" "), 40, ms, 0]).split(" ")[1];

function game(openingUci, newIsWhite) {
  const p = new Position(START_FEN);
  const moves = [];
  for (const u of openingUci.split(" ")) {
    p.make(p.legalMoves().find((x) => moveToUci(x) === u));
    moves.push(u);
  }
  for (let ply = 0; ply < 300; ply++) {
    const st = p.status();
    if (st !== "playing") {
      if (st === "checkmate") return (p.turn === WHITE) === newIsWhite ? -1 : 1; // the side to move is the mated one
      return 0;
    }
    const whiteToMove = moves.length % 2 === 0;
    const eng = whiteToMove === newIsWhite ? next : base;
    const u = search(eng, START_FEN, moves);
    const mv = p.legalMoves().find((x) => moveToUci(x) === u);
    if (!mv) return whiteToMove === newIsWhite ? -1 : 1; // an illegal or missing move loses
    p.make(mv);
    moves.push(u);
  }
  return 0;
}

let w = 0, d = 0, l = 0;
for (const o of OPENINGS) {
  for (const white of [true, false]) {
    const r = game(o, white);
    if (r > 0) w++; else if (r < 0) l++; else d++;
    console.log(`${o} new as ${white ? "white" : "black"}: ${r > 0 ? "win" : r < 0 ? "loss" : "draw"}   total +${w} =${d} -${l}`);
  }
}
const n = w + d + l;
console.log(`\nnew engine v baseline at ${ms} ms/move: +${w} =${d} -${l}  score ${((w + d / 2) / n * 100).toFixed(1)}%`);
