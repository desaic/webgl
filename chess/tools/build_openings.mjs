// Builds web/public/openings.json: position -> opening name, for the "opening" box in the UI.
//
//   node tools/build_openings.mjs ignore/openings      (folder with a.tsv ... e.tsv from
//   https://github.com/lichess-org/chess-openings, columns: eco, name, pgn)
//
// Every named line is replayed and its final position stored by Position.keyHash(), so the UI
// recognises an opening however the game got there (transpositions). Lines longer than MAX_PLY
// are skipped to keep the file small; where several lines end in the same position the first
// listed (shorter name) wins.
import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { Position, START_FEN } from "../web/src/rules.ts";
import { resolve, movetextTokens } from "./san.mjs";

const MAX_PLY = 14;
const dir = process.argv[2];
if (!dir) {
  console.error("usage: node tools/build_openings.mjs <folder with a.tsv..e.tsv>");
  process.exit(1);
}

const table = new Map();
let rows = 0, tooLong = 0, unreadable = 0;
for (const f of ["a", "b", "c", "d", "e"]) {
  const lines = readFileSync(join(dir, f + ".tsv"), "utf8").replace(/\r/g, "").split("\n").slice(1);
  for (const line of lines) {
    if (!line.trim()) continue;
    const [eco, name, pgn] = line.split("\t");
    rows++;
    const tokens = movetextTokens(pgn);
    if (tokens.length > MAX_PLY) { tooLong++; continue; }
    const pos = new Position(START_FEN);
    let ok = true;
    for (const t of tokens) {
      const m = resolve(pos, t);
      if (!m) { ok = false; break; }
      pos.make(m);
    }
    if (!ok) { unreadable++; continue; }
    const key = pos.keyHash();
    if (!table.has(key)) table.set(key, [key, eco, name, tokens.length]);
  }
}
// [hash, eco, name, ply]
const entries = [...table.values()].sort((a, b) => a[0] - b[0]);
writeFileSync("web/public/openings.json", JSON.stringify(entries));
console.log(`${rows} named lines, ${tooLong} longer than ${MAX_PLY} plies skipped, ${unreadable} unreadable, ${entries.length} positions written`);
