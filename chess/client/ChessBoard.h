#pragma once

#include <array>
#include <stdint.h>
#include <string>
#include <vector>
#include <list>

#define BOARD_SIZE 8

enum class PieceType
{
  EMPTY = 0, PAWN=1, ROOK = 2, KNIGHT, BISHOP,
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

  ChessCoord(const std::string& s) {
    Set(s);
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

  void Set(const std::string& s) {
    uint8_t x, y;
    x = s[0] - 'a';
    y = s[1] - '1';
    Set(x, y);
  }

  bool operator==(const ChessCoord& b) const{
    return coord == b.coord;
  }

  bool operator == (uint8_t b) const {
    return coord == b;
  }

  bool operator != (uint8_t b) const {
    return coord != b;
  }

  std::string toString() {
    std::string s;
    s.resize(2);
    s[0] = Col() + 'a';
    s[1] = Row() + '1';
    return s;
  }

  uint8_t coord;
};

///ran out of good names.
struct PieceInfo
{
  uint8_t info;
  PieceInfo() :info(0) {}

  PieceInfo(uint8_t i) :info(i) {}

  uint8_t color()const {
    return info >> 3;
  }

  uint8_t type()const {
    return info & 7;
  }

  void SetColor(PieceColor c) {
    info = (info & (~8)) | (uint8_t(c) << 3);
  }

  void SetType(PieceType t) {
    info = (info & (~7)) | uint8_t(t);
  }

  bool operator == (const PieceInfo& b)const {
    return info == b.info;
  }
  
  bool operator == (uint8_t b) const {
    return info == b;
  }

  bool operator != (uint8_t b) const {
    return info != b;
  }

  void operator = (uint8_t b) {
    info = b;
  }

  bool isEmpty() const {
    return info == 0;
  }

  void clear() {
    info = 0;
  }
};

///2 bytes in total.
///1 byte position and 1 byte type and color
struct Piece {
  ///3 bits type 1 bit color
  PieceInfo info;
  ChessCoord pos;
  Piece() :info(0) {}

  uint8_t color()const {
    return info.color();
  }
  
  uint8_t type()const {
    return info.type();
  }

  void SetColor(PieceColor c) {
    info.SetColor(c);;
  }

  void SetType(PieceType t) {
    info.SetType(t);
  }

  void clear() {
    info = 0;
  }

  bool isEmpty() const {
    return info.isEmpty();
  }
};

PieceInfo Char2PieceInfo(char c);

char PieceFEN(const PieceInfo& info);

struct Move
{
  ChessCoord src;
  ChessCoord dst;
  //promote to piece. usually none.
  PieceInfo promo;

  Move(){}

  Move(const ChessCoord& from, const ChessCoord& to):
  src(from), dst(to){
  }

  Move(const std::string& from, const std::string& to):
    src(from), dst(to){    
  }

  Move(const std::string& from, const std::string& to,
    uint8_t prom) :
    src(from), dst(to) {
    SetPromo(prom);
  }

  void SetPromo(char c) {
    promo = Char2PieceInfo(c);
  }

  bool operator==(const Move& b) {
    return src == b.src && dst == b.dst && promo == b.promo;
  }

  std::string toString() {
    std::string s = src.toString() + " " + dst.toString();
    if (!promo.isEmpty()) {
      Piece p;
      p.info = promo;
      char c = PieceFEN(p.info);
      s = s + " " + c;
    }
    return s;
  }
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
  ChessCoord enPassantDst;

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

  Piece* GetPiece(const ChessCoord & c) {
    return &(board[c.coord]);
  }

  Piece* GetPiece(uint8_t c) {
    return &(board[c]);
  }

  bool AddPiece(unsigned x, unsigned y, const Piece& piece);

  bool RemovePiece(unsigned x, unsigned y);

  void Clear();

  std::vector<Move> GetMoves();

  ///\return 0 on success.
  ///-1 or some negative error code if move is very invalid or something.
  /// does not check every kind of illegal move for efficiency and
  /// simplicity.
  int ApplyMove(const Move & m);

  void SetStartPos();

  ///\return 0 on success
  int FromFen(const std::string& fen);

  std::string GetFen();

private:
	void GetEvasions(std::vector<Move>& moves);
	void GetKingEvasions(std::vector<Move>& moves);

	void GetNonEvasions(std::vector<Move>& moves);
	void GetCaptures(std::vector<Move>& moves);
	void GetQuiets(std::vector<Move>& moves);
  void GetQuiets(std::vector<Move>& moves, const Piece & p);
  void GetQuietsPawn(std::vector<Move>& moves, const Piece& p);

	int numChecks();
};
