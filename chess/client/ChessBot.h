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
  size_t moveIdx;
  UndoMove undo;
  int score;
  SearchArg();
};

///dump intermediate evaluation related 
/// stuff here
struct EvalCache
{
  //current depth = argStack.size()
  //depth = 0 at the beginning of a new search.
  std::vector<SearchArg> argStack;
  unsigned depth;
  Move bestMove;
  int bestScore;
  int maxDepth;
  void Init();
  ChessBoard* board;
  EvalCache();
};

class ChessBot
{
public:
  static const int MAX_SCORE;
  static const int CASTLE_SCORE;
  std::vector<int> materialScore;

  ChessBot();
  ~ChessBot();
  
  ///score from the point of view of the next color to play.
  ///eval the board only based on current position and without any search.
  ///does not understand mate here. mate is handled in alpha beta search
  int EvalDirect(const ChessBoard& b);

  void SetBoard(ChessBoard& b);

  int CheckMateScore();

  int StaleMateScore();

  ///finds best moves at a given position
  std::vector<MoveScore> BestMoves(const ChessBoard& b);

  bool running;
  void Run();
  void Stop();
  void WorkerLoop();
  
  /// initialize evaluation.
  void InitEval();
  
  /// run evaluation for 1 step whatever that means.
  void EvalStep();
  
  /// get best move even if search is not finished.
  Move CurrentBestMove();

  std::thread searchThread;
  
  ///used whenever current position updates.
  std::mutex boardMutex;
  ChessBoard board;
  std::mutex cacheMutex;
  EvalCache cache;
  bool boardChanged;
};
