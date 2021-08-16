#pragma once
#include "ChessBoard.h"

class BoardHash
{
public:
  uint64_t hash;
  BoardHash() :hash(0) {}
  void Init(const ChessBoard& b)
  {

  }
  void Update(const ChessBoard& b, const Move & m){}
};