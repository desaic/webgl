// Which opening the game is in, for the box above the move list.
export type OpeningRow = [hash: number, eco: string, name: string, ply: number];

export interface DetectedOpening {
  eco?: string; // set only when the position is a named opening
  name?: string;
  line: string; // the moves that make up the opening, "1. e4 e5 2. Nf3 Nc6"
}

// "1. e4 e5 2. Nf3" for the first n half-moves
export function moveLine(sans: string[], n: number): string {
  let s = "";
  for (let i = 0; i < n; i++) s += (i % 2 === 0 ? (i ? " " : "") + (i / 2 + 1) + ". " : " ") + sans[i];
  return s;
}

// plyKeys[i] is the position hash after i half-moves (0 = the start); sans are the moves played.
// The name is that of the latest position in the game that is a named opening. The moves shown are
// the game's moves up to that position or up to the last book move (bookDepth), whichever is later.
// Returns null when the game is neither in a named opening nor in the book.
export function detectOpening(
  openings: Map<number, OpeningRow>,
  plyKeys: number[],
  sans: string[],
  bookDepth: number,
): DetectedOpening | null {
  let namedAt = -1;
  for (let i = plyKeys.length - 1; i > 0 && namedAt < 0; i--) if (openings.has(plyKeys[i])) namedAt = i;
  const shown = Math.min(sans.length, Math.max(bookDepth, namedAt));
  if (shown <= 0) return null;
  const line = moveLine(sans, shown);
  if (namedAt < 0) return { line };
  const row = openings.get(plyKeys[namedAt])!;
  return { eco: row[1], name: row[2], line };
}
