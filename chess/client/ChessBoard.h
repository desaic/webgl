#pragma once

#include <array>
#include <stdint.h>
#include <string>
#include <vector>
#include <list>

#define BOARD_SIZE 8

enum class PieceType
{
  EMPTY = 0, PAWN=1, ROOK = 2, KNIGHT=3, BISHOP=4,
  QUEEN=5, KING=6
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
  
  ChessCoord(uint8_t c) :coord(c) {}

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
  
  void IncRow() {
    coord += 8;
  }

  void DecRow() {
    coord -= 8;
  }

  void IncCol() {
    coord ++;
  }

  void DecCol() {
    coord --;
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

  bool inBound() {
    return coord < 64;
  }

  uint8_t coord;
};

struct Piece
{
  uint8_t info;
  Piece() :info(0) {}

  Piece(uint8_t i) :info(i) {}

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

  bool operator == (const Piece& b)const {
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

Piece Char2Piece(char c);

char PieceFEN(const Piece& info);

struct Move
{
  ChessCoord src;
  ChessCoord dst;
  //promote to piece. usually none.
  Piece promo;

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
    promo = Char2Piece(c);
  }

  bool operator==(const Move& b) {
    return src == b.src && dst == b.dst && promo == b.promo;
  }

  std::string toString() {
    std::string s = src.toString() + " " + dst.toString();
    if (!promo.isEmpty()) {
      char c = PieceFEN(promo);
      s = s + " " + c;
    }
    return s;
  }
};

///1 bit per position. 
struct BitBoard
{
  int64_t bits;
  BitBoard() :bits(0) {}
  
  uint8_t GetBit(uint8_t idx) {
    return (bits >> idx) & 1;
  }
  
  uint8_t GetBit(uint8_t x, uint8_t y) {
    return GetBit( (x | (y << 3)) );
  }

  uint8_t GetBit(ChessCoord c) {
    return GetBit(c.coord);
  }

  void SetBit(uint8_t i) {
    bits |= (1ll << i);
  }

  void SetBit(ChessCoord c) {
    SetBit(c.coord);
  }

  void ClearBit(uint8_t i) {
    bits &= (~(1ll << i));
  }
};

///infomation about checks
struct ChecksInfo {
  ChessCoord kingCoord;
  BitBoard attacked;
  
  //pieces that directly attacks the king
  std::vector<ChessCoord> attackers;

  //pieces pinning pieces to the king.
  //for a pinned piece at coordinate c
  //pinner[c] is pinning it.
  std::vector<ChessCoord> pinners;
  //bit set for dst in pins structure.
  //redundant for quicker lookup.
  BitBoard blockers;
};

class ChessBoard {

public:
  ChessBoard(); 
  
  std::vector<ChessCoord> black;
  std::vector<ChessCoord> white;
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

  Piece* GetPiece(ChessCoord c) {
    return &(board[c.coord]);
  }

  Piece* GetPiece(uint8_t c) {
    return &(board[c]);
  }

  bool AddPiece(ChessCoord c, Piece p);

  bool RemovePiece(ChessCoord c);

  ///moves a piece from src to dst and update the piece lists.
  ///does not care rules. does nothing if src is empty.
  /// Removes dst piece if there is any.
  bool MovePiece(ChessCoord src, ChessCoord dst);

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
  std::vector<ChessCoord>* GetPieceList(PieceColor c);

  void GetEvasions(std::vector<Move>& moves);
	void GetKingEvasions(std::vector<Move>& moves);
  ///generate blocking/capturing moves when there is only 1
  ///attacker checking the king.
  void GetBlockingMoves(std::vector<Move>& moves);
  std::vector<ChessCoord> GetBlockingPawn(ChessCoord c, bool canUseEnPassant);
  std::vector<ChessCoord> GetDstRook(ChessCoord c);
  std::vector<ChessCoord> GetDstKnight(ChessCoord c);

	void GetNonEvasions(std::vector<Move>& moves);
	void GetCaptures(std::vector<Move>& moves);
  void GetCaptures(std::vector<Move>& moves, ChessCoord c);
  void GetCapturesPawn(std::vector<Move>& moves, ChessCoord c);
  void GetCapturesRook(std::vector<Move>& moves, ChessCoord c);
  void GetCapturesKnight(std::vector<Move>& moves, ChessCoord c);
  void GetCapturesBishop(std::vector<Move>& moves, ChessCoord c);
  void GetCapturesQueen(std::vector<Move>& moves, ChessCoord c);
  void GetCapturesKing(std::vector<Move>& moves, ChessCoord c);

  void AddBlackPawnCaptures(ChessCoord src, ChessCoord dst, std::vector<Move>& moves);
  void AddWhitePawnCaptures(ChessCoord src, ChessCoord dst, std::vector<Move>& moves);

	void GetQuiets(std::vector<Move>& moves);
  void GetQuiets(std::vector<Move>& moves, ChessCoord c);
  void GetQuietsPawn(std::vector<Move>& moves, ChessCoord c);
  void GetQuietsRook(std::vector<Move>& moves, ChessCoord c);
  void GetQuietsKnight(std::vector<Move>& moves, ChessCoord c);
  void GetQuietsBishop(std::vector<Move>& moves, ChessCoord c);
  void GetQuietsQueen(std::vector<Move>& moves, ChessCoord c);
  void GetQuietsKing(std::vector<Move>& moves, ChessCoord c);
  void GetCastleBlack(std::vector<Move>& moves);
  void GetCastleWhite(std::vector<Move>& moves);

  void ComputeChecksRook(ChecksInfo& info, ChessCoord coord, PieceColor color,
    ChessCoord kingCoord);
  void ComputeChecksBishop(ChecksInfo& checks, ChessCoord coord, PieceColor color,
    ChessCoord kingCoord);
  void ComputeChecksQueen(ChecksInfo& checks, ChessCoord coord, PieceColor color,
    ChessCoord kingCoord);
  void ComputeChecksRay(ChecksInfo& checks, ChessCoord coord, PieceColor color,
    ChessCoord kingCoord, char dx, char dy);
  ChecksInfo ComputeChecks();

  ChecksInfo checksInfo;
};
