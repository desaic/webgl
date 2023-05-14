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

  MoveScore() :score(0), depth(0) {}
};

struct SearchConf {
  unsigned maxDepth = 6;
  ChessBoard board;
};

struct SearchState {
  Move bestMove;
  int score;
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
