// The 20 most popular openings, as full lines. The engine picks one at random and follows its side
// of the line while the game keeps to it, so it does not open the same way every game.
import { Position, START_FEN, moveToUci } from "./rules.ts";

export interface OpeningPlan {
  name: string;
  moves: string[]; // coordinate notation, both sides
}

const LINES: [name: string, san: string][] = [
  ["Ruy Lopez", "e4 e5 Nf3 Nc6 Bb5 a6 Ba4 Nf6 O-O Be7 Re1 b5 Bb3 d6"],
  ["Italian Game", "e4 e5 Nf3 Nc6 Bc4 Bc5 c3 Nf6 d3 d6 O-O O-O"],
  ["Scotch Game", "e4 e5 Nf3 Nc6 d4 exd4 Nxd4 Nf6 Nxc6 bxc6 e5 Qe7 Qe2 Nd5"],
  ["Petrov Defense", "e4 e5 Nf3 Nf6 Nxe5 d6 Nf3 Nxe4 d4 d5 Bd3 Be7 O-O Nc6"],
  ["Sicilian Defense: Najdorf", "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6 Be3 e5 Nb3 Be6"],
  ["Sicilian Defense: Dragon", "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 g6 Be3 Bg7 f3 O-O"],
  ["Sicilian Defense: Sveshnikov", "e4 c5 Nf3 Nc6 d4 cxd4 Nxd4 Nf6 Nc3 e5 Ndb5 d6 Bg5 a6"],
  ["French Defense: Classical", "e4 e6 d4 d5 Nc3 Nf6 Bg5 Be7 e5 Nfd7 Bxe7 Qxe7"],
  ["Caro-Kann Defense", "e4 c6 d4 d5 Nc3 dxe4 Nxe4 Bf5 Ng3 Bg6 h4 h6 Nf3 Nd7"],
  ["Scandinavian Defense", "e4 d5 exd5 Qxd5 Nc3 Qa5 d4 Nf6 Nf3 c6 Bc4 Bf5"],
  ["Pirc Defense", "e4 d6 d4 Nf6 Nc3 g6 Nf3 Bg7 Be2 O-O O-O"],
  ["Queen's Gambit Declined", "d4 d5 c4 e6 Nc3 Nf6 Bg5 Be7 e3 O-O Nf3 h6 Bh4 b6"],
  ["Queen's Gambit Accepted", "d4 d5 c4 dxc4 Nf3 Nf6 e3 e6 Bxc4 c5 O-O a6"],
  ["Slav Defense", "d4 d5 c4 c6 Nf3 Nf6 Nc3 dxc4 a4 Bf5 e3 e6 Bxc4 Bb4"],
  ["King's Indian Defense", "d4 Nf6 c4 g6 Nc3 Bg7 e4 d6 Nf3 O-O Be2 e5 O-O Nc6"],
  ["Nimzo-Indian Defense", "d4 Nf6 c4 e6 Nc3 Bb4 e3 O-O Bd3 d5 Nf3 c5 O-O Nc6"],
  ["Grünfeld Defense", "d4 Nf6 c4 g6 Nc3 d5 cxd5 Nxd5 e4 Nxc3 bxc3 Bg7 Nf3 c5"],
  ["London System", "d4 d5 Bf4 Nf6 e3 e6 Nf3 c5 c3 Nc6 Nbd2 Bd6 Bg3 O-O"],
  ["English Opening", "c4 e5 Nc3 Nf6 Nf3 Nc6 g3 d5 cxd5 Nxd5 Bg2 Nb6 O-O Be7"],
  ["Réti Opening", "Nf3 d5 g3 Nf6 Bg2 e6 O-O Be7 d3 O-O Nbd2 c5 e4 Nc6"],
];

// SAN to coordinate moves. Throws if a line contains an illegal move, so a typo cannot go unnoticed.
export function compilePlans(): OpeningPlan[] {
  return LINES.map(([name, san]) => {
    const pos = new Position(START_FEN);
    const moves: string[] = [];
    for (const token of san.split(" ")) {
      const m = pos.moveFromSan(token);
      if (!m) throw new Error(`opening plan "${name}": illegal or ambiguous move ${token} after ${moves.join(" ")}`);
      moves.push(moveToUci(m));
      pos.make(m);
    }
    return { name, moves };
  });
}

// Which plans fit a game so far (the moves played match the plan's first moves and the plan has a
// next move to play)?
export function fittingPlans(plans: OpeningPlan[], played: string[]): OpeningPlan[] {
  return plans.filter((p) => p.moves.length > played.length && played.every((u, i) => p.moves[i] === u));
}
