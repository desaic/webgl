#include "ChessBot.h"

static void SleepMs(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

const int ChessBot::MAX_SCORE = 10000;

ChessBot::ChessBot():running(false)
{
  materialScore.resize( unsigned(PieceType::NUM_TYPES) );
  materialScore[unsigned(PieceType::PAWN)] = 1;
  materialScore[unsigned(PieceType::KNIGHT)] = 3;
  materialScore[unsigned(PieceType::BISHOP)] = 3;
  materialScore[unsigned(PieceType::ROOK)] = 5;
  materialScore[unsigned(PieceType::QUEEN)] = 9;
  materialScore[unsigned(PieceType::KING)] = 0;
  Run();
}

ChessBot::~ChessBot()
{
  Stop();
}

float evalDirect(const ChessBoard& b)
{
  float score = 0.0f;

  return score;
}

std::vector<MoveScore> ChessBot::BestMoves(const ChessBoard& b)
{
  std::vector<MoveScore> moves;

  return moves;
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
