#include "ChessBot.h"

static void SleepMs(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

const int ChessBot::MAX_SCORE = 10000;

const int ChessBot::CASTLE_SCORE = 10;

//position score heuristics from white perspective
int pawnpos[64] =
{
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0, -5, -5,  0,  0,  0,
  2,  2,  3,  4,  4,  3,  2,  2,
  6,  4,  6, 10, 10,  6,  4,  6,
  8,  6,  9, 10, 10,  9,  6,  8,
  4,  8, 12, 16, 16, 12,  8,  4,
  50, 50, 50, 50, 50, 50, 50,  50,
  700,  700,  700,  700, 700, 700,700,700
};

int pawnpos_late[64] =
{
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0, -5, -5,  0,  0,  0,
  0,  2,  3,  4,  4,  3,  2,  0,
  0,  4,  6, 10, 10,  6,  4,  0,
  100, 100, 100, 100, 100, 100, 100,  100,
  200, 200, 200, 200, 200, 200, 200,  200,
  300, 300, 300, 300, 300, 300, 300,  300,
  800,  800,  800,  800, 800, 800,800,800
};

int knightpos[64] =
{
  -10, -5, -5, -5, -5, -5, -5,-10,
   -8,  0,  0,  3,  3,  0,  0, -8,
   -8,  0, 10,  8,  8, 10,  0, -8,
   -8,  0,  8, 10, 10,  8,  0, -8,
   -8,  0,  8, 10, 10,  8,  0, -8,
   -8,  0, 10,  8,  8, 10,  0, -8,
   -8,  0,  0,  3,  3,  0,  0, -8,
  -10, -5, -5, -5, -5, -5, -5,-10
};

int bishoppos[64] =
{
   -5, -5, -5, -5, -5, -5, -5, -5,
   -5, 10,  5,  8,  8,  5, 10, -5,
   -5,  5,  3,  8,  8,  3,  5, -5,
   -5,  3, 10,  3,  3, 10,  3, -5,
   -5,  3, 10,  3,  3, 10,  3, -5,
   -5,  5,  3,  8,  8,  3,  5, -5,
   -5, 10,  5,  8,  8,  5, 10, -5,
   -5, -5, -5, -5, -5, -5, -5, -5
};

ChessBot::ChessBot():running(false)
{
  materialScore.resize( unsigned(PieceType::NUM_TYPES) );
  materialScore[unsigned(PieceType::PAWN)] = 100;
  materialScore[unsigned(PieceType::KNIGHT)] = 300;
  materialScore[unsigned(PieceType::BISHOP)] = 300;
  materialScore[unsigned(PieceType::ROOK)] = 500;
  materialScore[unsigned(PieceType::QUEEN)] = 900;
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
