// Shared PGN/SAN helpers for the tools: read a SAN move against the legal moves of a position.
// the legal move a SAN token (check marks and annotations already removed) stands for, or null
export function resolve(pos, token) {
  return pos.moveFromSan(token);
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

