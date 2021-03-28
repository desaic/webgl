#pragma once
#include "ChessBoard.h"

#include <mutex>
#include <thread>

class ChessBot
{
public:

  ChessBot();
  ~ChessBot();
  
  float eval(const ChessBoard& b);

  Move bestMove(const ChessBoard& b);

  bool running;
  void Run();
  void Stop();
  void WorkerLoop();
  
  std::thread searchThread;
  
  ///used whenever current position updates.
  std::mutex boardMutex;
};