#include "ChessBot.h"
#include "ChessBoard.h"
#include "BoardEval.h"

#include "Timer.h"
#include <chrono>

static void SleepMs(int ms)
{
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

ChessBot::ChessBot():running(false),
boardChanged(false)
{
  cache.board = &workingBoard;
}

ChessBot::~ChessBot()
{
  Stop();
}

int ChessBot::EvalDirect(const ChessBoard& b)
{
  int score = 0;
  score = BoardEval::Eval(b);
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
    return BoardEval::MAX_SCORE;
  }
  else {
    return -BoardEval::MAX_SCORE;
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

EvalCache::EvalCache() :maxDepth(5)
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
  cache.initFen = cache.board->GetFen();
}

///https://en.wikipedia.org/wiki/Principal_variation_search
int ChessBot::SearchMoves()
{
  
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
  size_t nodeBatchSize = 100000;
  bool searchFinished = false;
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
        searchFinished = false;
        std::cout << "Bot receives new board position\n";
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

SearchArg::SearchArg(): alpha(-2 * BoardEval::MAX_SCORE),
      beta(2 * BoardEval::MAX_SCORE),
  moveIdx(0),
  score(0)
{
}

SearchArg::SearchArg(float a, float b) :
  alpha(a), beta(b),
  moveIdx(0), score(0),
  fullSearch(false)
{}