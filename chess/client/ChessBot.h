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

struct SearchArg
{
public:
  int alpha, beta;
  std::vector<Move> moves;
  ///for debug
  std::string initFen;
  size_t moveIdx;
  UndoMove undo;
  int score;
  bool fullSearch;
  SearchArg();
  SearchArg(float alpha, float beta);
};

///dump intermediate evaluation related 
/// stuff here
struct EvalCache
{
  //current depth = argStack.size()
  //depth = 0 at the beginning of a new search.
  std::vector<SearchArg> stack;
  int depth;
  Move bestMove;
  int bestScore;
  int maxDepth;
  void Init();
  ChessBoard* board;
  ///debugging
  std::string initFen;
  EvalCache();
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

  void SetMaxDepth(int d) { cache.maxDepth = d; }
  
  /// initialize evaluation.
  void InitEval();
  
  int SearchMoves();
  
  /// get best move even if search is not finished.
  Move CurrentBestMove();

  std::thread searchThread;
  
  ///used whenever current position updates.
  std::mutex boardMutex;
  ChessBoard board;
  ///board used by search
  ChessBoard workingBoard;
  std::mutex cacheMutex;
  EvalCache cache;
  bool boardChanged;
};
