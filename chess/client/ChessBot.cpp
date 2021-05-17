#include "ChessBot.h"

static void SleepMs(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

const int ChessBot::MAX_SCORE = 100000;

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

ChessBot::ChessBot():running(false),
boardChanged(false)
{
  materialScore.resize(PIECE_NUM_TYPES);
  materialScore[PIECE_PAWN] = 100;
  materialScore[PIECE_KNIGHT] = 300;
  materialScore[PIECE_BISHOP] = 300;
  materialScore[PIECE_ROOK] = 500;
  materialScore[PIECE_QUEEN] = 900;
  materialScore[PIECE_KING] = 0;
  Run();
}

ChessBot::~ChessBot()
{
  Stop();
}

int ChessBot::EvalDirect(const ChessBoard& b)
{
  int score = 0;
  //sum of rook knight bishop queen.
  //some copying some old code. not sure why it helps.
  int blackRNBQ = 0, whiteRNBQ = 0;

  ChessCoord blackKing, whiteKing;
  int blackSum = 0, whiteSum = 0;
  //black pieces.
  for (size_t i = 0; i < b.pieces[0].size(); i++) {
    ChessCoord c = b.pieces[0][i];
    ChessCoord blackc(c.Col(), 7 - c.Row());
    const Piece* piece = b.GetPiece(c);
    uint8_t type = piece->type();
    blackSum += materialScore[type];
    switch (type) {
    case PIECE_PAWN:
      blackSum += pawnpos[blackc.coord];
      break;
    case PIECE_ROOK:
      blackRNBQ++;
      break;
    case PIECE_KNIGHT:
      blackSum += knightpos[blackc.coord];
      blackRNBQ++;
      break;
    case PIECE_BISHOP:
      blackSum += bishoppos[blackc.coord];
      blackRNBQ++;
      break;
    case PIECE_QUEEN:
      blackRNBQ += 2;
      break;
    case PIECE_KING:
      blackKing = c;
      break;
    }
  }

  //white pieces.
  for (size_t i = 0; i < b.pieces[1].size(); i++) {
    ChessCoord c = b.pieces[1][i];
    const Piece* piece = b.GetPiece(c);
    uint8_t type = piece->type();
    whiteSum += materialScore[type];
    switch (type) {
    case PIECE_PAWN:
      whiteSum += pawnpos[c.coord];
      break;
    case PIECE_ROOK:
      whiteRNBQ++;
      break;
    case PIECE_KNIGHT:
      whiteSum += knightpos[c.coord];
      whiteRNBQ++;
      break;
    case PIECE_BISHOP:
      whiteSum += bishoppos[c.coord];
      whiteRNBQ++;
      break;
    case PIECE_QUEEN:
      whiteRNBQ += 2;
      break;
    case PIECE_KING:
      whiteKing = c;
      break;
    }
  }

  if (b.hasCastled[0]) {
    blackSum += CASTLE_SCORE;
  }
  if (b.hasCastled[1]) {
    whiteSum += CASTLE_SCORE;
  }
  int blackRNBQval, whiteRNBQval;
  if (whiteRNBQ != 0) {
    blackRNBQval = blackRNBQ * 100 / whiteRNBQ;
  }
  else{
    blackRNBQval = 255 * blackRNBQ;
  }
  if (blackRNBQ != 0) {
    whiteRNBQval = whiteRNBQ * 100 / blackRNBQ;
  }
  else {
    whiteRNBQval = 255 * whiteRNBQ;
  }
  score = whiteSum - blackSum;
  score += whiteRNBQval - blackRNBQval;
  if (b.nextColor == PIECE_BLACK) {
    score = -score;
  }
  return score;
}

void ChessBot::SetBoard(ChessBoard& b)
{
  std::lock_guard<std::mutex> lock(boardMutex);
  board = b;
  boardChanged = true;
}

int ChessBot::CheckMateScore()
{
  return MAX_SCORE;
}

int ChessBot::StaleMateScore()
{
  return 0;
}

std::vector<MoveScore> ChessBot::BestMoves(const ChessBoard& b)
{
  std::vector<MoveScore> moves;

  return moves;
}

void ChessBot::WorkerLoop()
{
  int pollInterval = 30;//ms
  
  ChessBoard localBoard = board;
  
  int printInterval = 2000;

  while (running) {
    boardMutex.lock();
    if (boardChanged) {
      localBoard = board;
      boardChanged = false;
    }
    boardMutex.unlock();


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
