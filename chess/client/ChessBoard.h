#pragma once

#include <array>
#include <stdint.h>
#include <string>
#include <vector>
#include <list>

#define BOARD_SIZE 8

enum class PieceType
{
  EMPTY = 0, PAWN=1, ROOK, KNIGHT, BISHOP,
  QUEEN, KING
};

enum class PieceColor {
  BLACK=0,
  WHITE=1
};

///machine friendly coordinates in [0,7]
///64 cells in total. 1 byte is enough
struct ChessCoord
{
  ChessCoord() :coord(0) {}
  ChessCoord(uint8_t x, uint8_t y) {
    Set(x, y);
  }
  uint8_t Row() const {
    return coord >> 3;
  }
  
  uint8_t Col()const {
    return coord & 7;
  }
  
  void Set(uint8_t x, uint8_t y) {
    coord = (y<<3) | (x);
  }

  bool operator==(const ChessCoord& b) {
    return coord == b.coord;
  }

  uint8_t coord;
};

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

///2 bytes in total.
///1 byte position and 1 byte type and color
struct Piece {
  ///3 bits type 1 bit color
  uint8_t info;
  ChessCoord pos;
  Piece() :info(0) {}
  uint8_t color()const {
    return info >> 3;
  }
  uint8_t type()const {
    return info & 7;
  }

  void SetColor(PieceColor c) {
    info = (info & (~8)) | ( uint8_t(c) << 3);
  }

  void SetType(PieceType t) {
    info = (info & (~7)) | uint8_t(t);
  }
};

struct Move
{
  ChessCoord src;
  ChessCoord dst;
  //promote to piece. usually none.
  uint8_t promo=0;
};

class ChessBoard {

public:
  ChessBoard(); 
  
  std::vector<Piece*> black;
  std::vector<Piece*> white;
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

private:
	void GetEvasions(std::vector<Move>& moves);
	void GetKingEvasions(std::vector<Move>& moves);

	void GetNonEvasions(std::vector<Move>& moves);
	void GetCaptures(std::vector<Move>& moves);
	void GetQuiets(std::vector<Move>& moves);

	int numChecks();
};
