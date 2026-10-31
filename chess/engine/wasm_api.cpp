// C API exported to JavaScript (see worker.ts). Everything is synchronous; the page runs it in a
// web worker so the UI thread is never blocked.
#include "ChessBoard.h"
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
// repetitions are seen. Returns "bestmove <uci|none> score <cp> depth <d> nodes <n> timeout <0|1>".
EMSCRIPTEN_KEEPALIVE const char* engine_search(const char* fen, const char* moves, int maxDepth,
                                               int timeMs) {
  Init();
  ChessBoard b;
  b.FromFen(fen);
  b.InitHash();
  std::istringstream in(moves ? moves : "");
  std::string tok;
  while (in >> tok) {
    if (!PlayUci(b, tok)) break;
  }
  SearchResult r = g_search.Think(b, maxDepth, timeMs);
  std::ostringstream out;
  out << "bestmove " << (r.hasMove ? Uci(r.best) : "none") << " score " << r.score << " depth "
      << r.depth << " nodes " << r.nodes << " timeout " << (r.timedOut ? 1 : 0);
  g_out = out.str();
  return g_out.c_str();
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

EMSCRIPTEN_KEEPALIVE double engine_perft(const char* fen, int depth) {
  Init();
  ChessBoard b;
  b.FromFen(fen);
  b.InitHash();
  return double(Perft(b, depth));
}

}
