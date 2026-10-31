// Run with: npm test   (node runs this file directly; build the engine first with ../engine/build.sh)
// 1. JS rules against published perft node counts.
// 2. JS legal moves against the C++ move generator on random playouts.
import { Position, START_FEN, moveToUci } from "./rules.ts";
import createEngine from "./engine/engine.mjs";

let failed = 0;
const check = (ok: boolean, msg: string) => {
  if (!ok) failed++;
  console.log((ok ? "ok    " : "FAIL  ") + msg);
};

// https://www.chessprogramming.org/Perft_Results
const cases: [string, string, number, number][] = [
  ["startpos", START_FEN, 4, 197281],
  ["kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3, 97862],
  ["pos3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624],
  ["pos4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333],
  ["pos4 mirrored", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4, 422333],
  ["pos5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 3, 62379],
  ["pos6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 3, 89890],
];
for (const [name, fen, depth, nodes] of cases) {
  const n = new Position(fen).perft(depth);
  check(n === nodes, `perft ${name} depth ${depth}: ${n} (expected ${nodes})`);
}

// FEN round trip
check(new Position(START_FEN).getFen() === START_FEN, "fen round trip");

// SAN spot checks
{
  const p = new Position("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1");
  const names = p.legalMoves().map((m) => p.san(m));
  check(names.includes("O-O") && names.includes("O-O-O") && names.includes("Rb1"), "san castling and rook moves");
}

// game-end detection
check(new Position("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1").status() === "stalemate", "stalemate");
check(new Position("6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1").legalMoves().length > 0, "not mate");
check(new Position("R5k1/5ppp/8/8/8/8/8/6K1 b - - 0 1").status() === "checkmate", "back rank mate");
check(new Position("8/8/4k3/8/8/3K4/8/8 w - - 0 1").status() === "insufficient", "K vs K");
{
  const p = new Position("4k3/8/8/8/8/8/8/R3K3 w - - 0 1");
  const mv = (u: string) => {
    const m = p.legalMoves().find((x) => moveToUci(x) === u)!;
    p.make(m);
  };
  for (let i = 0; i < 2; i++) { mv("a1a2"); mv("e8e7"); mv("a2a1"); mv("e7e8"); }
  check(p.status() === "repetition", "threefold repetition");
}

// cross-check against the C++ engine
const engine = await createEngine();
const cppMoves = (fen: string): string[] =>
  engine.ccall("engine_legal_moves", "string", ["string"], [fen]).trim().split(" ").filter(Boolean).sort();
let seed = 12345;
const rnd = () => ((seed = (seed * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff);
let positions = 0, mismatches = 0;
for (let game = 0; game < 40; game++) {
  const p = new Position(game % 2 ? cases[1][1] : START_FEN);
  for (let ply = 0; ply < 120; ply++) {
    const moves = p.legalMoves();
    const js = moves.map(moveToUci).sort();
    const cpp = cppMoves(p.getFen());
    positions++;
    if (js.join() !== cpp.join()) {
      mismatches++;
      if (mismatches < 4) console.log("mismatch at", p.getFen(), "\n js", js.join(" "), "\n c++", cpp.join(" "));
    }
    if (moves.length === 0 || p.status() !== "playing") break;
    p.make(moves[Math.floor(rnd() * moves.length)]);
  }
}
check(mismatches === 0, `JS vs C++ legal moves on ${positions} random-playout positions (${mismatches} mismatches)`);
for (const [name, fen, depth] of cases.slice(0, 3)) {
  const js = new Position(fen).perft(Math.min(depth, 3));
  const cpp = engine.ccall("engine_perft", "number", ["string", "number"], [fen, Math.min(depth, 3)]);
  check(js === cpp, `perft ${name} JS ${js} == C++ ${cpp}`);
}

process.exit(failed ? 1 : 0);
