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

class ChessBot
{
public:
  static const int MAX_SCORE;
  static const int CASTLE_SCORE;
  std::vector<int> materialScore;

  ChessBot();
  ~ChessBot();
  
  //eval the board only based on current position and without any search.
  float evalDirect(const ChessBoard& b);

  ///finds best moves at a given position
  std::vector<MoveScore> BestMoves(const ChessBoard& b);

  bool running;
  void Run();
  void Stop();
  void WorkerLoop();
  
  std::thread searchThread;
  
  ///used whenever current position updates.
  std::mutex boardMutex;
};