// C API exported to JavaScript (see worker.ts). Everything is synchronous; the page runs it in a
// web worker so the UI thread is never blocked.
#include "ChessBoard.h"
#include "Book.h"
#include "BoardEval.h"
#include "Search.h"

#include <emscripten.h>

#include <cctype>
#include <cstdio>
#include <sstream>
#include <string>

static Search g_search;
static std::string g_out;
static bool g_init = false;

static void Init() {
  if (g_init) return;
  BoardHash::Init();
  g_init = true;
}

static std::string Uci(const Move& m) {
  std::string s = m.src.ToString() + m.dst.ToString();
  if (!m.promo.isEmpty()) s += char(std::tolower(PieceFEN(m.promo)));
  return s;
}

// plays a coordinate move such as "e2e4" or "e7e8q" if it is legal. Returns false otherwise.
static bool PlayUci(ChessBoard& b, const std::string& s) {
  if (s.size() < 4) return false;
  for (const Move& m : b.GetMoves()) {
    if (Uci(m) == s) {
      b.ApplyMove(m);
      return true;
    }
  }
  return false;
}

static uint64_t Perft(ChessBoard& b, int depth) {
  std::vector<Move> moves = b.GetMoves();
  if (depth <= 1) return moves.size();
  uint64_t n = 0;
  for (const Move& m : moves) {
    UndoMove u = b.GetUndoMove(m);
    b.ApplyMove(m);
    n += Perft(b, depth - 1);
    b.Undo(u);
  }
  return n;
}

extern "C" {

// Position is a FEN plus the moves played since (space separated, coordinate notation) so
// repetitions are seen. With useBook, a position in the opening book is answered from the book
// (a random one of the moves strong players chose there, weighted by how often) without searching.
// Returns "bestmove <uci|none> score <cp> depth <d> nodes <n> timeout <0|1> book <games>" where
// games is how many source games played the chosen book move, 0 if the move was searched.
EMSCRIPTEN_KEEPALIVE const char* engine_search(const char* fen, const char* moves, int maxDepth,
                                               int timeMs, int useBook) {
  Init();
  ChessBoard b;
  b.FromFen(fen);
  b.InitHash();
  std::istringstream in(moves ? moves : "");
  std::string tok;
  while (in >> tok) {
    if (!PlayUci(b, tok)) break;
  }
  std::ostringstream out;
  BookChoice choice;
  if (useBook && BookPick(b, b.GetMoves(), choice)) {
    out << "bestmove " << Uci(choice.move) << " score 0 depth 0 nodes 0 timeout 0 book " << choice.games;
  } else {
    SearchResult r = g_search.Think(b, maxDepth, timeMs);
    out << "bestmove " << (r.hasMove ? Uci(r.best) : "none") << " score " << r.score << " depth "
        << r.depth << " nodes " << r.nodes << " timeout " << (r.timedOut ? 1 : 0) << " book 0";
  }
  g_out = out.str();
  return g_out.c_str();
}

// How many of the given moves (from the FEN position) are still opening book moves: the length of
// the game's "theory" prefix. Stops at the first move that is not in the book.
EMSCRIPTEN_KEEPALIVE int engine_book_depth(const char* fen, const char* moves) {
  Init();
  ChessBoard b;
  b.FromFen(fen);
  b.InitHash();
  std::istringstream in(moves ? moves : "");
  std::string tok;
  int depth = 0;
  while (in >> tok) {
    Move chosen;
    bool found = false;
    for (const Move& m : b.GetMoves()) {
      if (Uci(m) == tok) {
        chosen = m;
        found = true;
        break;
      }
    }
    if (!found || !BookHasMove(b, PackMove(chosen))) break;
    b.ApplyMove(chosen);
    depth++;
  }
  return depth;
}

EMSCRIPTEN_KEEPALIVE void engine_new_game() {
  Init();
  g_search.ClearTable();
}

// legal moves of a position as "e2e4 e2e3 ..." (used to cross-check the JS rules)
EMSCRIPTEN_KEEPALIVE const char* engine_legal_moves(const char* fen) {
  Init();
  ChessBoard b;
  b.FromFen(fen);
  b.InitHash();
  g_out.clear();
  for (const Move& m : b.GetMoves()) g_out += Uci(m) + " ";
  return g_out.c_str();
}

// static evaluation of a position in centipawns from the point of view of the side to move
// (no search), for tests and diagnostics
EMSCRIPTEN_KEEPALIVE int engine_eval(const char* fen) {
  Init();
  ChessBoard b;
  b.FromFen(fen);
  b.InitHash();
  return BoardEval::Eval(b);
}

EMSCRIPTEN_KEEPALIVE double engine_perft(const char* fen, int depth) {
  Init();
  ChessBoard b;
  b.FromFen(fen);
  b.InitHash();
  return double(Perft(b, depth));
}

}
