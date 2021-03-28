#pragma once

#include <array>
#include <string>
#include <vector>

#define CHESS_BLACK 0
#define CHESS_WHITE 1

enum class ChessPiece
{
  PAWN=1, ROOK, KNIGHT, BISHOP,
  QUEEN, KING
};

///machine friendly coordinates in [0,7]
struct Vec2u8 {
  unsigned char v[2];
  Vec2u8(){
    v[0] = 0;
    v[1] = 0;
  }

  Vec2u8(unsigned x, unsigned y) {
    v[0] = x;
    v[1] = y;
  }

  unsigned char operator[](unsigned i) const {
    return v[i];
  }

  unsigned char &operator[](unsigned i) {
    return v[i];
  }
};

struct Move
{
  Vec2u8 src;
  Vec2u8 dst;
};

class ChessBoard {

public:
  ChessBoard(); 

  std::vector<ChessPiece> black;
  std::vector<ChessPiece> white;

  int nextColor;
  
  bool castleBk;
  bool castleBQ;
  bool castleWK;
  bool castleWQ;

  bool hasEnPassant;

  ///En passant target square.
  Vec2u8 enPassantDst;

  ///number of half moves since last capture or pawn advance for
  ///drawing rule
  int halfMove;

  ///number of full moves.
  ///starts at 1 and increments after every black move.
  int fullMoves;

  std::vector<Move> GetMoves();

  ///\return 0 on success.
  ///-1 or some negative error code if move is invalid or something.
  int ApplyMove(const Move & m);

  ///\return 0 on success
  int FromFen(const std::string& fen);

  std::string GetFen();
};
