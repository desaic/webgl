#pragma once
#include "ChessBoard.h"

#include <chrono>
#include <vector>

struct SearchResult {
  Move best;
  bool hasMove = false;
  int score = 0;       // centipawns from the side to move; |score| near MateScore means mate
  int depth = 0;       // last fully completed iteration
  uint64_t nodes = 0;
  bool timedOut = false;
};

// Classic fixed-depth negamax alpha-beta with quiescence, wrapped in iterative deepening.
// The search polls a deadline and unwinds when it passes, so the caller always gets the best
// move of the last completed depth within the time budget.
class Search {
 public:
  static const int MateScore = 100000;
  static const int MaxPly = 64;

  Search();
  void ClearTable();

  // timeMs <= 0 means no time limit (maxDepth alone bounds the search).
  SearchResult Think(ChessBoard& board, int maxDepth, int timeMs);

 private:
  struct Entry {
    uint64_t hash = 0;
    int score = 0;
    uint16_t move = 0;
    int8_t depth = -1;
    uint8_t bound = 0;  // 0 exact, 1 upper, 2 lower
  };

  int Negamax(ChessBoard& b, int depth, int ply, int alpha, int beta);
  int Quiesce(ChessBoard& b, int ply, int alpha, int beta);
  bool OutOfTime();
  bool IsRepetition(const ChessBoard& b) const;
  void OrderMoves(const ChessBoard& b, std::vector<Move>& moves, uint16_t ttMove, int ply) const;

  std::vector<Entry> table_;
  uint64_t tableMask_;
  Move killers_[MaxPly][2];
  bool stopped_;
  bool timed_;
  uint64_t nodes_;
  std::chrono::steady_clock::time_point deadline_;
};
