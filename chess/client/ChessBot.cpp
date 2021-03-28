#include "ChessBot.h"

static void SleepMs(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

ChessBot::ChessBot():running(false)
{
  Run();
}

ChessBot::~ChessBot()
{
  Stop();
}

float ChessBot::eval(const ChessBoard& b)
{
  return 0.0f;
}

Move ChessBot::bestMove(const ChessBoard& b)
{
  Move m;

  return m;
}

void ChessBot::WorkerLoop()
{
  int pollInterval = 30;//ms
  while (running) {
    
    SleepMs(running);
  }
}

void ChessBot::Run()
{
  if (searchThread.joinable()) {
    return;
  }
  
  running = true;

  searchThread = std::thread(&ChessBot::WorkerLoop, this);  
}

void ChessBot::Stop()
{
  running = false;
  if (searchThread.joinable()) {
    searchThread.join();
  }
}
