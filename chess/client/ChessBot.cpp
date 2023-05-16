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
  conf.board = board;
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

void ChessBot::InitEval() 
{
  conf.board = board;
}

/// https://en.wikipedia.org/wiki/Principal_variation_search
int ChessBot::pvs(ChessBoard& board, unsigned depth, int alpha, int beta) {
  
  if(
  
  std::vector<Move> moves = conf.board.GetMoves();
  if (moves.size() == 0) {
    if (board.IsInCheck()) {
      return -BoardEval::MAX_SCORE;
    } else {
      return 0;
    }
  }

  if(depth == 0){
    return BoardEval::Eval(board);
  }

  return 0;
}

int ChessBot::SearchMoves()
{
  int alpha = -2*BoardEval::MAX_SCORE;
  int beta = 2 * BoardEval::MAX_SCORE;
  int score = 0;
  for (int depth = 1; depth < conf.maxDepth; depth++) {
    score = pvs(conf.board, conf.maxDepth, alpha, beta);
  }
  return score;
}

Move ChessBot::CurrentBestMove()
{
  std::lock_guard<std::mutex> lock(stateMutex);
  return state.bestMove;
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
    SearchMoves();
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
