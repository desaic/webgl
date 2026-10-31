// Shared PGN/SAN helpers for the tools: read a SAN move against the legal moves of a position.
import { typeOf, fileOf, rankOf, squareName, P, N, B, R, Q, K } from "../web/src/rules.ts";

const PIECE = { N, B, R, Q, K };

// the legal move a SAN token (check marks and annotations already removed) stands for, or null
export function resolve(pos, token) {
  const legal = pos.legalMoves();
  if (token === "O-O" || token === "0-0") return legal.find((m) => typeOf(pos.board[m.from]) === K && fileOf(m.to) - fileOf(m.from) === 2) ?? null;
  if (token === "O-O-O" || token === "0-0-0") return legal.find((m) => typeOf(pos.board[m.from]) === K && fileOf(m.from) - fileOf(m.to) === 2) ?? null;
  const m = /^([NBRQK])?([a-h])?([1-8])?x?([a-h][1-8])(?:=?([NBRQ]))?$/.exec(token);
  if (!m) return null;
  const type = m[1] ? PIECE[m[1]] : P;
  const cands = legal.filter((mv) => {
    if (typeOf(pos.board[mv.from]) !== type) return false;
    if (squareName(mv.to) !== m[4]) return false;
    if (m[2] && "abcdefgh"[fileOf(mv.from)] !== m[2]) return false;
    if (m[3] && String(rankOf(mv.from) + 1) !== m[3]) return false;
    if ((mv.promo || 0) !== (m[5] ? PIECE[m[5]] : 0)) return false;
    return true;
  });
  return cands.length === 1 ? cands[0] : null;
}

export function movetextTokens(text) {
  const t = text
    .replace(/\{[^}]*\}/g, " ") // comments
    .replace(/;[^\n]*/g, " ")
    .replace(/\([^)]*\)/g, " ") // variations (no nesting in these files)
    .replace(/\$\d+/g, " ")
    .replace(/\d+\.(\.\.)?/g, " "); // move numbers
  return t.split(/\s+/).filter((x) => x && !/^(1-0|0-1|1\/2-1\/2|\*)$/.test(x)).map((x) => x.replace(/[+#!?]+$/, ""));
}

