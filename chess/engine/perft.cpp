// Move generator validation against published perft node counts
// (https://www.chessprogramming.org/Perft_Results).
#include "ChessBoard.h"

#include <cstdio>

static uint64_t Perft(ChessBoard& b, int depth) {
  std::vector<Move> moves = b.GetMoves();
  if (depth == 1) {
    return moves.size();
  }
  uint64_t n = 0;
  for (const Move& m : moves) {
    UndoMove u = b.GetUndoMove(m);
    b.ApplyMove(m);
    n += Perft(b, depth - 1);
    b.Undo(u);
  }
  return n;
}

struct Case {
  const char* name;
  const char* fen;
  int depth;
  uint64_t nodes;
};

int main() {
  BoardHash::Init();
  const Case cases[] = {
      {"startpos", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 5, 4865609},
      {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4, 4085603},
      {"pos3", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5, 674624},
      {"pos4", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 4, 422333},
      {"pos4 mirrored", "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 4, 422333},
      {"pos5", "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 4, 2103487},
      {"pos6", "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 4, 3894594},
  };
  int failed = 0;
  for (const Case& c : cases) {
    ChessBoard b;
    if (b.FromFen(c.fen) != 0) {
      printf("%-14s bad fen\n", c.name);
      failed++;
      continue;
    }
    b.InitHash();
    uint64_t n = Perft(b, c.depth);
    bool ok = n == c.nodes;
    failed += !ok;
    printf("%-14s depth %d  %llu  expected %llu  %s\n", c.name, c.depth,
           (unsigned long long)n, (unsigned long long)c.nodes, ok ? "ok" : "FAIL");
    fflush(stdout);
  }
  return failed ? 1 : 0;
}
