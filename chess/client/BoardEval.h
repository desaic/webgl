#ifndef BOARD_EVAL_H
#define BOARD_EVAL_H
#include "ChessBoard.h"

class BoardEval {
public:
  static const int MAX_SCORE;

  static const int CASTLE_SCORE;

  static int Eval(const ChessBoard& b);

  // neither side can force mate (bare kings, one minor piece, K+NN v K)
  static bool InsufficientMaterial(const ChessBoard& b);

};

#endif