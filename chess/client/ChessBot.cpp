#include "ChessBot.h"
#include "ChessBoard.h"
#include "Timer.h"
#include <chrono>

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

  cache.board = &workingBoard;

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

int ChessBot::CheckMateScore(uint8_t color)
{
  if (color == PIECE_WHITE) {
    return MAX_SCORE;
  }
  else {
    return -MAX_SCORE;
  }
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

EvalCache::EvalCache() :maxDepth(10)
{

}

void EvalCache::Init()
{
  stack.clear();
  bestScore = 0;
  depth = 0;
}

void ChessBot::InitEval() 
{
  cache.Init();
  workingBoard = board;
  // create stack and moves for root node 
  cache.stack.push_back(SearchArg());
  cache.stack[0].moves = cache.board->GetMoves();

}

///https://en.wikipedia.org/wiki/Principal_variation_search
int ChessBot::EvalStep()
{
  std::lock_guard<std::mutex> lock(cacheMutex);
  int d = cache.depth;

  if (d < 0) {
    //nothing more to search.
    return -1;
  }

  // leaf node
  if (d >= cache.maxDepth) {
    cache.stack[d].score = EvalDirect(*(cache.board));
    cache.depth--;
    return 0;
  }

  SearchArg * arg = &( cache.stack[d]);
  std::vector<Move>* moves = &(cache.stack[d].moves);
  if (moves->size() == 0) {
    if (cache.board->IsInCheck()) {
      // if I'm in check, the other player wins
      arg->score = CheckMateScore((1 - cache.board->nextColor));
    }
    else {
      arg->score = StaleMateScore();
    }
    cache.depth--;
    return 0;
  }

  if (d + 2 < cache.stack.size()) {
    //serious bug
    std::cout << "debug\n";
  }

  // process return value from child node
  // set up search for the next child node.
  // free child node stack when no more child left to search.
  if (d + 1 < cache.stack.size()) {
    int score = -cache.stack[d + 1].score;
    if (arg->moveIdx > 0 && arg->alpha < score < arg->beta
      && !(cache.stack[d + 1].fullSearch)) {
      //full re-search 
      cache.depth++;
      std::vector<Move> moves = cache.stack[d + 1].moves;
      cache.stack[d + 1] = SearchArg(-arg->beta, -score);
      cache.stack[d + 1].fullSearch = true;
      cache.stack[d + 1].moves = moves;
      return 0;
    }
    //update current best move at root level.
    if (d == 0 && score > arg->alpha) {
      cache.bestMove = (*moves)[arg->moveIdx];
      cache.bestScore = score;
      std::cout << "move " << cache.bestMove.ToString() << "score " << score << "\n";
    }
    arg->alpha = std::max(arg->alpha, score);

    cache.board->Undo(arg->undo);
    //beta cutoff
    if (arg->alpha >= arg->beta) {
      arg->score = arg->alpha;
      arg->moveIdx = moves->size();
    }
    arg->moveIdx++;
    //search child nodes after the first child
    cache.stack[d+1]=SearchArg(-arg->alpha - 1, -arg->alpha);
  }
  else {
    //for debugging.
    //something in tree traversal algo went really wrong.
    if (d + 1 != cache.stack.size()) {
      return -2;
    }
    //just entered this node. moves and child stack hasn't been allocated.
    //set up args for the first child
    cache.stack.push_back(SearchArg(-arg->beta, -arg->alpha));
    //stack may have been reallocated.
    arg = &(cache.stack[d]);
    moves = &(arg->moves);
  }

  if (arg->moveIdx >= moves->size()) {
    //remove any child stack.
    while (cache.stack.size() > d + 1) {
      cache.stack.pop_back();
    }
    cache.depth--;
    //score = alpha
    cache.stack[d].score = cache.stack[d].alpha;
    return 0;
  }
  
  Move m = (*moves)[arg->moveIdx];
  arg->undo = cache.board->GetUndoMove(m);
  cache.board->ApplyMove(m);
  cache.stack[d + 1].moves = cache.board->GetMoves();
  cache.depth++;
  return 0;
}

Move ChessBot::CurrentBestMove()
{
  std::lock_guard<std::mutex> lock(cacheMutex);
  return cache.bestMove;
}

void ChessBot::WorkerLoop()
{
  int pollIntervalMs = 100;//ms

  float printIntervalSec = 2;
  Timer timer;
  timer.start();

  Timer pollTimer;
  pollTimer.start();
  while (running) {
    //EvalStep();
    float sec = timer.elapsedSeconds();
    if (sec > printIntervalSec) {
      //std::cout << "best move\n";
      //std::cout << cache.bestMove.ToString() << " " << cache.bestScore << "\n";
      timer.start();
    }
    
    float pollSec = pollTimer.elapsedSeconds();
    if (pollSec > pollIntervalMs / 1000.0f) {
      boardMutex.lock();
      if (boardChanged) {
        boardChanged = false;
        InitEval();
      }
      boardMutex.unlock();
      pollTimer.start();
    }
    SleepMs(pollIntervalMs);
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

SearchArg::SearchArg():
  alpha(-ChessBot::MAX_SCORE),
  beta(ChessBot::MAX_SCORE),
  moveIdx(0),
  score(0)
{
}

SearchArg::SearchArg(float a, float b) :
  alpha(a), beta(b),
  moveIdx(0), score(0),
  fullSearch(false)
{}