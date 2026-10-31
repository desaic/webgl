// Chess rules for the UI: legal move generation, check/mate/draw detection, FEN.
// Squares are 0..63 with a1 = 0, h1 = 7, a8 = 56 (same layout as the C++ engine).
// Validated by rules.test.ts against published perft counts and the C++ move generator.

export const P = 1, N = 2, B = 3, R = 4, Q = 5, K = 6;
export const WHITE = 0, BLACK = 8; // piece = type | color
export const START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

export interface Move {
  from: number;
  to: number;
  promo: number; // 0 or N/B/R/Q
}

const FEN_CHARS = ".PNBRQK";
const KNIGHT = [[1, 2], [2, 1], [2, -1], [1, -2], [-1, -2], [-2, -1], [-2, 1], [-1, 2]];
const KING = [[1, 0], [1, 1], [0, 1], [-1, 1], [-1, 0], [-1, -1], [0, -1], [1, -1]];
const DIAG = [[1, 1], [1, -1], [-1, 1], [-1, -1]];
const ORTHO = [[1, 0], [-1, 0], [0, 1], [0, -1]];

export const sq = (file: number, rank: number) => rank * 8 + file;
export const fileOf = (s: number) => s & 7;
export const rankOf = (s: number) => s >> 3;
export const typeOf = (p: number) => p & 7;
export const colorOf = (p: number) => p & 8;
export const squareName = (s: number) => "abcdefgh"[fileOf(s)] + (rankOf(s) + 1);
export const parseSquare = (n: string) => sq(n.charCodeAt(0) - 97, n.charCodeAt(1) - 49);

export function moveToUci(m: Move): string {
  return squareName(m.from) + squareName(m.to) + (m.promo ? FEN_CHARS[m.promo].toLowerCase() : "");
}

interface Undo {
  move: Move;
  captured: number;
  capSq: number;
  castling: number;
  ep: number;
  halfmove: number;
  rookFrom: number;
  rookTo: number;
}

// castling bits
const WK = 1, WQ = 2, BK = 4, BQ = 8;

export class Position {
  board = new Array<number>(64).fill(0);
  turn = WHITE;
  castling = 0;
  ep = -1; // en passant target square, only kept when a pawn can actually capture
  halfmove = 0;
  fullmove = 1;
  private undo: Undo[] = [];
  // position keys (without clocks) of every position reached, for repetition detection
  private keys: string[] = [];

  constructor(fen = START_FEN) {
    this.setFen(fen);
  }

  setFen(fen: string) {
    const parts = fen.trim().split(/\s+/);
    this.board.fill(0);
    let rank = 7, file = 0;
    for (const c of parts[0]) {
      if (c === "/") { rank--; file = 0; }
      else if (c >= "1" && c <= "8") file += +c;
      else {
        const t = FEN_CHARS.indexOf(c.toUpperCase());
        this.board[sq(file++, rank)] = t | (c === c.toUpperCase() ? WHITE : BLACK);
      }
    }
    this.turn = parts[1] === "b" ? BLACK : WHITE;
    const c = parts[2] || "-";
    this.castling = (c.includes("K") ? WK : 0) | (c.includes("Q") ? WQ : 0) |
      (c.includes("k") ? BK : 0) | (c.includes("q") ? BQ : 0);
    this.ep = parts[3] && parts[3] !== "-" ? parseSquare(parts[3]) : -1;
    this.halfmove = +(parts[4] ?? 0) || 0;
    this.fullmove = +(parts[5] ?? 1) || 1;
    this.undo = [];
    this.keys = [];
    this.fixEp();
    this.keys.push(this.key());
  }

  getFen(): string {
    let s = "";
    for (let r = 7; r >= 0; r--) {
      let empty = 0;
      for (let f = 0; f < 8; f++) {
        const p = this.board[sq(f, r)];
        if (!p) { empty++; continue; }
        if (empty) { s += empty; empty = 0; }
        const ch = FEN_CHARS[typeOf(p)];
        s += colorOf(p) === WHITE ? ch : ch.toLowerCase();
      }
      if (empty) s += empty;
      if (r) s += "/";
    }
    const c = (this.castling & WK ? "K" : "") + (this.castling & WQ ? "Q" : "") +
      (this.castling & BK ? "k" : "") + (this.castling & BQ ? "q" : "");
    const side = this.turn === WHITE ? "w" : "b";
    const ep = this.ep >= 0 ? squareName(this.ep) : "-";
    return s + " " + side + " " + (c || "-") + " " + ep + " " + this.halfmove + " " + this.fullmove;
  }

  // repetition key: placement, side, castling, en passant
  private key(): string {
    return this.getFen().split(" ").slice(0, 4).join(" ");
  }

  // keep the en passant square only if a pawn of the side to move can really capture there
  private fixEp() {
    if (this.ep < 0) return;
    const pawn = P | this.turn;
    const dir = this.turn === WHITE ? -1 : 1;
    const r = rankOf(this.ep) + dir;
    let ok = false;
    for (const df of [-1, 1]) {
      const f = fileOf(this.ep) + df;
      if (f >= 0 && f < 8 && this.board[sq(f, r)] === pawn) ok = true;
    }
    if (!ok) this.ep = -1;
  }

  attacked(square: number, by: number): boolean {
    const f = fileOf(square), r = rankOf(square);
    const at = (ff: number, rr: number) => (ff >= 0 && ff < 8 && rr >= 0 && rr < 8 ? this.board[sq(ff, rr)] : 0);
    const pr = by === WHITE ? r - 1 : r + 1; // pawns attack toward the enemy
    if (at(f - 1, pr) === (P | by) || at(f + 1, pr) === (P | by)) return true;
    for (const [df, dr] of KNIGHT) if (at(f + df, r + dr) === (N | by)) return true;
    for (const [df, dr] of KING) if (at(f + df, r + dr) === (K | by)) return true;
    for (const [dirs, t] of [[DIAG, B], [ORTHO, R]] as const) {
      for (const [df, dr] of dirs) {
        let ff = f + df, rr = r + dr;
        while (ff >= 0 && ff < 8 && rr >= 0 && rr < 8) {
          const p = this.board[sq(ff, rr)];
          if (p) {
            if (colorOf(p) === by && (typeOf(p) === t || typeOf(p) === Q)) return true;
            break;
          }
          ff += df; rr += dr;
        }
      }
    }
    return false;
  }

  kingSquare(color: number): number {
    return this.board.indexOf(K | color);
  }

  inCheck(color = this.turn): boolean {
    const k = this.kingSquare(color);
    return k >= 0 && this.attacked(k, color ^ 8);
  }

  private pseudoMoves(): Move[] {
    const out: Move[] = [];
    const us = this.turn, them = us ^ 8;
    const add = (from: number, to: number, promo = 0) => out.push({ from, to, promo });
    for (let s = 0; s < 64; s++) {
      const p = this.board[s];
      if (!p || colorOf(p) !== us) continue;
      const t = typeOf(p), f = fileOf(s), r = rankOf(s);
      if (t === P) {
        const dir = us === WHITE ? 1 : -1;
        const startRank = us === WHITE ? 1 : 6;
        const lastRank = us === WHITE ? 7 : 0;
        const push = (to: number) => {
          if (rankOf(to) === lastRank) for (const pr of [Q, R, B, N]) add(s, to, pr);
          else add(s, to);
        };
        const one = sq(f, r + dir);
        if (!this.board[one]) {
          push(one);
          if (r === startRank && !this.board[sq(f, r + 2 * dir)]) add(s, sq(f, r + 2 * dir));
        }
        for (const df of [-1, 1]) {
          if (f + df < 0 || f + df > 7) continue;
          const to = sq(f + df, r + dir);
          const tp = this.board[to];
          if ((tp && colorOf(tp) === them) || to === this.ep) push(to);
        }
      } else if (t === N || t === K) {
        for (const [df, dr] of t === N ? KNIGHT : KING) {
          const ff = f + df, rr = r + dr;
          if (ff < 0 || ff > 7 || rr < 0 || rr > 7) continue;
          const tp = this.board[sq(ff, rr)];
          if (!tp || colorOf(tp) === them) add(s, sq(ff, rr));
        }
      } else {
        const dirs = t === B ? DIAG : t === R ? ORTHO : DIAG.concat(ORTHO);
        for (const [df, dr] of dirs) {
          let ff = f + df, rr = r + dr;
          while (ff >= 0 && ff < 8 && rr >= 0 && rr < 8) {
            const tp = this.board[sq(ff, rr)];
            if (!tp) add(s, sq(ff, rr));
            else {
              if (colorOf(tp) === them) add(s, sq(ff, rr));
              break;
            }
            ff += df; rr += dr;
          }
        }
      }
    }
    // castling: squares between must be empty, king may not start in, cross or end in check
    const home = us === WHITE ? 0 : 56;
    if (this.board[home + 4] === (K | us) && !this.attacked(home + 4, them)) {
      const ks = us === WHITE ? WK : BK, qs = us === WHITE ? WQ : BQ;
      if (this.castling & ks && this.board[home + 7] === (R | us) && !this.board[home + 5] &&
        !this.board[home + 6] && !this.attacked(home + 5, them) && !this.attacked(home + 6, them)) {
        add(home + 4, home + 6);
      }
      if (this.castling & qs && this.board[home] === (R | us) && !this.board[home + 1] &&
        !this.board[home + 2] && !this.board[home + 3] && !this.attacked(home + 3, them) &&
        !this.attacked(home + 2, them)) {
        add(home + 4, home + 2);
      }
    }
    return out;
  }

  legalMoves(): Move[] {
    const us = this.turn;
    return this.pseudoMoves().filter((m) => {
      this.make(m);
      const ok = !this.inCheck(us);
      this.unmake();
      return ok;
    });
  }

  legalMovesFrom(from: number): Move[] {
    return this.legalMoves().filter((m) => m.from === from);
  }

  isPromotion(from: number, to: number): boolean {
    const p = this.board[from];
    return typeOf(p) === P && (rankOf(to) === 7 || rankOf(to) === 0);
  }

  make(m: Move) {
    const b = this.board;
    const p = b[m.from];
    const us = colorOf(p);
    const u: Undo = {
      move: m, captured: b[m.to], capSq: m.to, castling: this.castling, ep: this.ep,
      halfmove: this.halfmove, rookFrom: -1, rookTo: -1,
    };
    if (typeOf(p) === P && m.to === this.ep) {
      u.capSq = sq(fileOf(m.to), rankOf(m.from));
      u.captured = b[u.capSq];
      b[u.capSq] = 0;
    }
    b[m.to] = m.promo ? m.promo | us : p;
    b[m.from] = 0;
    if (typeOf(p) === K && Math.abs(fileOf(m.to) - fileOf(m.from)) === 2) {
      const home = rankOf(m.from) * 8;
      u.rookFrom = home + (m.to > m.from ? 7 : 0);
      u.rookTo = home + (m.to > m.from ? 5 : 3);
      b[u.rookTo] = b[u.rookFrom];
      b[u.rookFrom] = 0;
    }
    // castling rights are lost when a king/rook moves or a rook is captured on its home square
    for (const s of [m.from, m.to]) {
      if (s === 4) this.castling &= ~(WK | WQ);
      else if (s === 60) this.castling &= ~(BK | BQ);
      else if (s === 7) this.castling &= ~WK;
      else if (s === 0) this.castling &= ~WQ;
      else if (s === 63) this.castling &= ~BK;
      else if (s === 56) this.castling &= ~BQ;
    }
    this.ep = typeOf(p) === P && Math.abs(m.to - m.from) === 16 ? (m.from + m.to) >> 1 : -1;
    this.halfmove = typeOf(p) === P || u.captured ? 0 : this.halfmove + 1;
    if (us === BLACK) this.fullmove++;
    this.turn ^= 8;
    this.fixEp();
    this.undo.push(u);
    this.keys.push(this.key());
  }

  unmake() {
    const u = this.undo.pop()!;
    this.keys.pop();
    const b = this.board;
    this.turn ^= 8;
    if (this.turn === BLACK) this.fullmove--;
    const m = u.move;
    const moved = b[m.to];
    b[m.from] = m.promo ? P | this.turn : moved;
    b[m.to] = 0;
    if (u.capSq !== m.to) b[u.capSq] = u.captured;
    else b[m.to] = u.captured;
    if (u.rookFrom >= 0) {
      b[u.rookFrom] = b[u.rookTo];
      b[u.rookTo] = 0;
    }
    this.castling = u.castling;
    this.ep = u.ep;
    this.halfmove = u.halfmove;
  }

  private insufficientMaterial(): boolean {
    const minors: number[] = [];
    for (let s = 0; s < 64; s++) {
      const t = typeOf(this.board[s]);
      if (!t || t === K) continue;
      if (t === P || t === R || t === Q) return false;
      minors.push(s);
    }
    if (minors.length <= 1) return true;
    // only bishops, all on the same square colour
    if (minors.every((s) => typeOf(this.board[s]) === B)) {
      const col = (s: number) => (fileOf(s) + rankOf(s)) & 1;
      return minors.every((s) => col(s) === col(minors[0]));
    }
    return false;
  }

  repetitions(): number {
    const k = this.keys[this.keys.length - 1];
    return this.keys.filter((x) => x === k).length;
  }

  // "playing", "checkmate", "stalemate", "fifty", "repetition", "insufficient"
  status(): string {
    if (this.legalMoves().length === 0) return this.inCheck() ? "checkmate" : "stalemate";
    if (this.insufficientMaterial()) return "insufficient";
    if (this.halfmove >= 100) return "fifty";
    if (this.repetitions() >= 3) return "repetition";
    return "playing";
  }

  // what kind of move a legal move is (ask before making it): used to pick the move sound
  kind(m: Move): "promotion" | "castle" | "capture" | "move" {
    const p = this.board[m.from];
    if (m.promo) return "promotion";
    if (typeOf(p) === K && Math.abs(fileOf(m.to) - fileOf(m.from)) === 2) return "castle";
    if (this.board[m.to] || (typeOf(p) === P && m.to === this.ep)) return "capture";
    return "move";
  }

  // standard algebraic notation of a legal move (call before making it)
  san(m: Move): string {
    const p = this.board[m.from];
    const t = typeOf(p);
    let s: string;
    if (t === K && Math.abs(fileOf(m.to) - fileOf(m.from)) === 2) {
      s = m.to > m.from ? "O-O" : "O-O-O";
    } else {
      const capture = this.board[m.to] || (t === P && m.to === this.ep);
      s = "";
      if (t === P) {
        if (capture) s += "abcdefgh"[fileOf(m.from)];
      } else {
        s += FEN_CHARS[t];
        const rivals = this.legalMoves().filter(
          (o) => o.to === m.to && o.from !== m.from && typeOf(this.board[o.from]) === t);
        if (rivals.length) {
          const sameFile = rivals.some((o) => fileOf(o.from) === fileOf(m.from));
          const sameRank = rivals.some((o) => rankOf(o.from) === rankOf(m.from));
          if (!sameFile) s += "abcdefgh"[fileOf(m.from)];
          else if (!sameRank) s += rankOf(m.from) + 1;
          else s += squareName(m.from);
        }
      }
      if (capture) s += "x";
      s += squareName(m.to);
      if (m.promo) s += "=" + FEN_CHARS[m.promo];
    }
    this.make(m);
    if (this.inCheck()) s += this.legalMoves().length ? "+" : "#";
    this.unmake();
    return s;
  }

  perft(depth: number): number {
    const moves = this.legalMoves();
    if (depth <= 1) return moves.length;
    let n = 0;
    for (const m of moves) {
      this.make(m);
      n += this.perft(depth - 1);
      this.unmake();
    }
    return n;
  }
}
