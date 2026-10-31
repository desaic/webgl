// Run with: npm test   (node runs this file directly; build the engine first with ../engine/build.sh)
// 1. JS rules against published perft node counts.
// 2. JS legal moves against the C++ move generator on random playouts.
import { Position, START_FEN, moveToUci, P, BLACK, WHITE } from "./rules.ts";
import createEngine from "./engine/engine.mjs";
import { readFileSync } from "node:fs";
import { detectOpening, type OpeningRow } from "./opening.ts";
import { compilePlans, fittingPlans } from "./openingPlans.ts";

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

// move kinds (they pick the move sound)
{
  const kind = (fen: string, uci: string) => {
    const p = new Position(fen);
    return p.kind(p.legalMoves().find((m) => moveToUci(m) === uci)!);
  };
  check(kind(START_FEN, "e2e4") === "move", "kind: plain move");
  check(kind("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1g1") === "castle", "kind: castle short");
  check(kind("r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", "e8c8") === "castle", "kind: castle long");
  check(kind("4k3/8/8/3p4/4P3/8/8/4K3 w - - 0 1", "e4d5") === "capture", "kind: capture");
  check(kind("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", "e5d6") === "capture", "kind: en passant capture");
  check(kind("8/P6k/8/8/8/8/8/4K3 w - - 0 1", "a7a8q") === "promotion", "kind: promotion");
  check(kind("1n5k/P7/8/8/8/8/8/4K3 w - - 0 1", "a7b8q") === "promotion", "kind: capturing promotion");
}

// opening detection (names from the generated openings.json)
{
  const rows: OpeningRow[] = JSON.parse(readFileSync("public/openings.json", "utf8"));
  const names = new Map(rows.map((r) => [r[0], r] as [number, OpeningRow]));
  const play = (ucis: string[]) => {
    const p = new Position(START_FEN);
    const keys = [p.keyHash()];
    const sans: string[] = [];
    for (const u of ucis) {
      const m = p.legalMoves().find((x) => moveToUci(x) === u)!;
      sans.push(p.san(m));
      p.make(m);
      keys.push(p.keyHash());
    }
    return { keys, sans };
  };
  const ruy = play(["e2e4", "e7e5", "g1f3", "b8c6", "f1b5"]);
  const d = detectOpening(names, ruy.keys, ruy.sans, 0);
  check(!!d && /Ruy Lopez/.test(d.name ?? "") && d.line === "1. e4 e5 2. Nf3 Nc6 3. Bb5", "named opening found: " + d?.eco + " " + d?.name);
  // the same position reached by a different move order is recognised too
  const trans = play(["g1f3", "b8c6", "e2e4", "e7e5", "f1b5"]);
  check(names.has(trans.keys[5]) && names.get(trans.keys[5])![2] === names.get(ruy.keys[5])![2], "transposition gives the same opening");
  // leaving the named line keeps the name of the last named position and still lists the moves
  const long = play(["e2e4", "e7e5", "g1f3", "b8c6", "f1b5", "a7a6", "b5a4", "g8f6", "e1g1", "f8e7", "f1e1", "b7b5", "a4b3", "d7d6", "c2c3", "e8g8"]);
  const dl = detectOpening(names, long.keys, long.sans, 16);
  check(!!dl && /Ruy Lopez/.test(dl.name ?? "") && dl.line.endsWith("8. c3 O-O"), "line continues past the last named position: " + dl?.line.slice(-28));
  // no name known: just the moves of the book line
  const unnamed = detectOpening(new Map(), ruy.keys, ruy.sans, 3);
  check(!!unnamed && unnamed.name === undefined && unnamed.eco === undefined && unnamed.line === "1. e4 e5 2. Nf3", "no name: only the moves are shown: " + unnamed?.line);
  check(detectOpening(new Map(), ruy.keys, ruy.sans, 0) === null, "no name and no book moves: nothing shown");
  check(detectOpening(names, [new Position(START_FEN).keyHash()], [], 0) === null, "start position: nothing shown");
}

// SAN reader and the popular opening lines
{
  const p = new Position("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
  let ok = 0, bad = 0;
  for (const m of p.legalMoves()) {
    const back = p.moveFromSan(p.san(m));
    if (back && back.from === m.from && back.to === m.to && back.promo === m.promo) ok++; else bad++;
  }
  check(bad === 0 && ok > 40, `moveFromSan(san(m)) returns m for all ${ok} legal moves of kiwipete`);
  const pr = new Position("1n5k/P7/8/8/8/8/8/4K3 w - - 0 1");
  check(pr.moveFromSan("axb8=Q+")?.promo === 5 && pr.moveFromSan("a8=N")?.promo === 2, "moveFromSan reads promotions");
  check(new Position(START_FEN).moveFromSan("Nf6") === null && new Position(START_FEN).moveFromSan("e5") === null, "moveFromSan rejects illegal moves");
  const plans = compilePlans(); // throws on an illegal move
  check(plans.length === 20 && plans.every((q) => q.moves.length >= 10), "20 opening plans compile to legal lines");
  const first = new Set(plans.map((q) => q.moves[0]));
  check(first.size === 4, "plans cover 1.e4, 1.d4, 1.c4 and 1.Nf3: " + [...first].join(" "));
  check(fittingPlans(plans, []).length === 20, "all 20 plans fit the start");
  const afterE4 = fittingPlans(plans, ["e2e4"]);
  check(afterE4.length > 5 && afterE4.every((q) => q.moves[0] === "e2e4"), "after 1.e4 only 1.e4 plans fit: " + afterE4.length);
  check(fittingPlans(plans, ["e2e4", "a7a5"]).length === 0, "an unexpected reply fits no plan");
  const replies = new Set(afterE4.map((q) => q.moves[1]));
  check(replies.size >= 5, "the engine as Black has several different replies to 1.e4: " + [...replies].join(" "));
}

// captured pieces
{
  const p = new Position(START_FEN);
  const play = (u: string) => p.make(p.legalMoves().find((m) => moveToUci(m) === u)!);
  check(p.capturedPieces().length === 0, "nothing captured at the start");
  for (const u of ["e2e4", "d7d5", "e4d5", "d8d5"]) play(u);
  const cap = p.capturedPieces();
  check(cap.length === 2 && cap[0] === (P | BLACK) && cap[1] === (P | WHITE), "captured: black pawn, then white pawn");
  p.unmake();
  check(p.capturedPieces().length === 1, "undo takes the capture back");
  const q = new Position("4k3/P7/8/8/8/8/8/4K3 w - - 0 1");
  q.make(q.legalMoves().find((m) => moveToUci(m) === "a7a8q")!);
  check(q.capturedPieces().length === 0, "a promotion is not a capture");
  const e = new Position("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1");
  e.make(e.legalMoves().find((m) => moveToUci(m) === "e5d6")!);
  check(e.capturedPieces().length === 1 && e.capturedPieces()[0] === (P | BLACK), "en passant captures the pawn");
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
