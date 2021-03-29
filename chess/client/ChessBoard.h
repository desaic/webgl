#pragma once

#include <array>
#include <stdint.h>
#include <string>
#include <vector>
#include <list>

#define CHESS_BLACK 0
#define CHESS_WHITE 1
#define BOARD_SIZE 8

enum class PieceType
{
  EMPTY = 0, PAWN=1, ROOK, KNIGHT, BISHOP,
  QUEEN, KING
};

enum class PieceColor {
  BLACK,
  WHITE
};

///machine friendly coordinates in [0,7]
struct Vec2u8 {
  uint8_t v[2];
  Vec2u8(){
    v[0] = 0;
    v[1] = 0;
  }

  Vec2u8(unsigned x, unsigned y) {
    v[0] = x;
    v[1] = y;
  }

  uint8_t operator[](unsigned i) const {
    return v[i];
  }

  uint8_t &operator[](unsigned i) {
    return v[i];
  }

  bool operator == (const Vec2u8& b) {
    return v[0] == b.v[0] && v[1] == b.v[1];
  }
};

///4 bytes in total. same as an int.
struct Piece {
  uint8_t type;
  uint8_t color;
  Vec2u8 pos;
  Piece() :type(0), color(0) {}
};

struct Move
{
  Vec2u8 src;
  Vec2u8 dst;
};

class ChessBoard {

public:
  ChessBoard(); 
  
  ///list for easy removal and insertion
  std::list<Piece*> black;
  std::list<Piece*> white;
  ///8x8 board with 64 squares
  std::vector<Piece> board;
  PieceColor nextColor;
  
  bool castleBK;
  bool castleBQ;
  bool castleWK;
  bool castleWQ;

  bool hasEnPassant;

  ///En passant target square.
  Vec2u8 enPassantDst;

  ///number of half moves since last capture or pawn advance for
  ///drawing rule
  int halfMoves;

  ///number of full moves.
  ///starts at 1 and increments after every black move.
  int fullMoves;

  Piece& operator()(unsigned x, unsigned y) {
    return board[x + y * BOARD_SIZE];
  }

  const Piece& operator()(unsigned x, unsigned y) const {
    return board[x + y * BOARD_SIZE];
  }

  Piece* GetPiece(unsigned x, unsigned y){
    return &(board[x + y * BOARD_SIZE]);
  }

  bool AddPiece(unsigned x, unsigned y, const Piece& piece);

  bool RemovePiece(unsigned x, unsigned y);

  void Clear();

  std::vector<Move> GetMoves();

  ///\return 0 on success.
  ///-1 or some negative error code if move is invalid or something.
  int ApplyMove(const Move & m);

  ///\return 0 on success
  int FromFen(const std::string& fen);

  std::string GetFen();
};
