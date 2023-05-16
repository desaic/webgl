#pragma once
#include "ChessBoard.h"

#include <mutex>
#include <thread>

struct MoveScore
{
  Move move;
  int score;
  //depth the score is based on.
  unsigned depth;

  bool operator<(const MoveScore& other) const {
    if (score != other.score) {
      return score < other.score;
    }
    return move < other.move;
  }

  MoveScore() :score(0), depth(0) {}
};

//depth 0 for uninitialized scores.
struct BoardScore {
  uint8_t hash[8];
  uint8_t depth=0;
  int score=0;
};

struct SearchConf {
  unsigned maxDepth = 6;
  ChessBoard board;
};

// transposition table.
// hash table with eviction only for 
// conflicts.
struct TransTable{
  std::vector<BoardScore> v;
  TransTable() { Allocate(20); }
  void Allocate(size_t numBits) {
    if (numBits > 32) {
      numBits = 32;
    }
    size_t size = 1ull << numBits;
    hashMask = (~1ull) >> (64 - numBits);
    v.resize(size);
  }

  const BoardScore& Get(size_t hash) const { size_t idx = hash & hashMask;
    return v[hash];
  }

  void Set(size_t hash, const BoardScore& score) {
    size_t idx = hash & hashMask;
    v[idx] = score;
  }

  BoardScore empty;
  size_t numBits=10;
  size_t hashMask = 0x3ff;
};

struct SearchState {
  Move bestMove;
  int score;
  TransTable tt;
};

class ChessBot
{
public:

  ChessBot();
  ~ChessBot();
  
  ///score from the point of view of the next color to play.
  ///eval the board only based on current position and without any search.
  ///does not understand mate here. mate is handled in alpha beta search
  int EvalDirect(const ChessBoard& b);

  void SetBoard(ChessBoard& b);

  int CheckMateScore(uint8_t color);

  int StaleMateScore();

  ///finds best moves at a given position
  std::vector<MoveScore> BestMoves(const ChessBoard& b);

  bool running;
  void Run();
  void Stop();
  void WorkerLoop();

  void SetMaxDepth(unsigned d) { conf.maxDepth = d; }
  
  /// initialize evaluation.
  void InitEval();
  
  int pvs(ChessBoard& board, unsigned detph, int alpha, int beta);

  int SearchMoves();
  
  /// get best move even if search is not finished.
  Move CurrentBestMove();

  std::thread searchThread;
  
  ///used whenever current position updates.
  std::mutex boardMutex;
  ChessBoard board;
  bool boardChanged;

  SearchConf conf;
  mutable std::mutex stateMutex;
  SearchState state;
};
