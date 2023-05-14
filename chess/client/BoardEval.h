#ifndef BOARD_EVAL_H
#define BOARD_EVAL_H
#include "ChessBoard.h"

class BoardEval {
public:
  static const int MAX_SCORE;

  static const int CASTLE_SCORE;

  static int Eval(const ChessBoard& b);

  static const int MAX_SCORE;

  static const int CASTLE_SCORE;
};

#endif