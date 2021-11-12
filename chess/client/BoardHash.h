#pragma once

#include <stdint.h>

#define NUM_SQUARES (64)
#define NUM_PIECES  (16)
#define CASTLING_OPTIONS (4)
#define ENPASSANT_OPTIONS (8)

class ChessBoard;
struct Move;
class BoardHash
{
public:
  uint64_t hash;
  BoardHash():
    hash(0) 
  {}

  ///sets up hash keys and rng
  static void Init();
  void Update(const ChessBoard& b, const Move& m);
  void Set(const ChessBoard& b);
private:
  static uint64_t positionTable[NUM_SQUARES][NUM_PIECES];
  static uint64_t castlingTable[CASTLING_OPTIONS];
  static uint64_t enpassantTable[ENPASSANT_OPTIONS];
  static uint64_t side;
};