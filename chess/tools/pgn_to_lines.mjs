// Turns PGN game collections into opening lines for the book builder (engine/make_book.cpp).
//
//   node tools/pgn_to_lines.mjs ignore/book_lines.txt F:/github/chess/twic1489.pgn F:/github/chess/twic920.pgn
//
// Keeps games where both players are rated at least MIN_ELO, and only the first MAX_PLY half-moves
// of each. Output: one game per line, moves in coordinate notation (e2e4 e7e5 ...).
import { readFileSync, writeFileSync } from "node:fs";
import { Position, START_FEN, moveToUci } from "../web/src/rules.ts";
import { resolve, movetextTokens } from "./san.mjs";

const MIN_ELO = 2300;
const MAX_PLY = 16;

const [out, ...inputs] = process.argv.slice(2);
if (!out || !inputs.length) {
  console.error("usage: node tools/pgn_to_lines.mjs <out.txt> <games.pgn>...");
  process.exit(1);
}

const lines = [];
let games = 0, kept = 0, bad = 0;
for (const file of inputs) {
  const text = readFileSync(file, "utf8").replace(/\r/g, "");
  // a game is its header block followed by its movetext, separated from the next by a blank line
  const blocks = text.split(/\n\n(?=\[Event )/);
  for (const block of blocks) {
    games++;
    const headers = Object.fromEntries([...block.matchAll(/^\[(\w+) "([^"]*)"\]/gm)].map((h) => [h[1], h[2]]));
    if (headers.FEN || headers.Variant) continue;
    if (!(+headers.WhiteElo >= MIN_ELO && +headers.BlackElo >= MIN_ELO)) continue;
    const moves = block.slice(block.lastIndexOf("]") + 1);
    const pos = new Position(START_FEN);
    const uci = [];
    for (const token of movetextTokens(moves)) {
      if (uci.length >= MAX_PLY) break;
      const m = resolve(pos, token);
      if (!m) { bad++; break; }
      uci.push(moveToUci(m));
      pos.make(m);
    }
    if (uci.length >= 4) {
      lines.push(uci.join(" "));
      kept++;
    }
  }
}
writeFileSync(out, lines.join("\n") + "\n");
console.log(`${games} games read, ${kept} kept (both players >= ${MIN_ELO}, first ${MAX_PLY} plies), ${bad} stopped early on an unreadable move`);
