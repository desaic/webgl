#include "ChessBoard.h"

#include <sstream>

ChessBoard::ChessBoard() :nextColor(PieceColor::WHITE),
castleBK(true),
castleBQ(true),
castleWK(true),
castleWQ(true),
hasEnPassant(false),
enPassantDst(0, 0),
halfMoves(0),
fullMoves(0)
{
  board.resize(64);
}

std::vector<Move> ChessBoard::GetMoves()
{
  std::vector<Move> moves;



  return moves;
}

int ChessBoard::ApplyMove(const Move& m)
{

  return 0;
}

char PieceFEN(const Piece& p)
{
  PieceType type = PieceType(p.type);
  char c;
  const char LOWER_DIFF = 'a' - 'A';
  switch (type) {
  case PieceType::PAWN:
    c = 'P';
    break;
  case PieceType::ROOK:
    c='R';
    break;
  case PieceType::KNIGHT:
    c='N';
    break;
  case PieceType::BISHOP:
    c='B';
    break;
  case PieceType::QUEEN:
    c='Q';
    break;
  case PieceType::KING:
    c='K';
    break;
  default:
    //unknown shouldn't really get here ever.
    c = 'U';
    break;
  }

  if (p.color == uint8_t(PieceColor::BLACK)) {
    c += LOWER_DIFF;
  }
  return c;
}

std::string coordString(const Vec2u8& coord)
{
  std::string s="  ";
  s[0] = 'a' + coord[0];
  s[1] = '0' + coord[1];
  return s;
}

int ChessBoard::FromFen(const std::string& fen)
{
  return 0;
}

std::string ChessBoard::GetFen()
{
  std::ostringstream oss;
  for (int row = BOARD_SIZE-1; row >= 0; row--) {
    int emptyCnt = 0;
    for (int col = 0; col < BOARD_SIZE; col++){
      const Piece& p = (*this)(col, row);
      if (p.type == uint8_t(PieceType::EMPTY)) {
        emptyCnt++;
      }
      else {
        if (emptyCnt > 0) {
          oss << emptyCnt;
        }
        char c = PieceFEN(p);
        oss << c;
        emptyCnt = 0;
      }
    }
    if (emptyCnt > 0) {
      oss << emptyCnt;
    }
    if (row > 0) {
      oss << "/";
    }
  }
  oss << " ";
  if (nextColor == PieceColor::WHITE) {
    oss << "w ";
  }
  else {
    oss << "b ";
  }

  bool hasCastle = false;
  if (castleWK) {
    oss << "K";
    hasCastle = true;
  }
  if (castleWQ) {
    oss << "Q";
    hasCastle = true;
  }
  if (castleBK) {
    oss << "k";
    hasCastle = true;
  }
  if (castleBQ) {
    oss << "q";
    hasCastle = true;
  }

  if (!hasCastle) {
    oss << "-";
  }
  oss << " ";

  if (hasEnPassant) {
    std::string s = coordString(enPassantDst);
    oss << s;
  }
  else {
    oss << '-';
  }
  oss << " ";
  oss << halfMoves << " ";
  oss << fullMoves;
  return oss.str();
  
}

bool ChessBoard::AddPiece(unsigned x, unsigned y, const Piece& piece)
{
  if ((*this)(x, y).type != uint8_t(PieceType::EMPTY)) {
    return false;
  }
  (*this)(x, y) = piece;
  (*this)(x, y).pos = Vec2u8(x, y);

  std::list<Piece*>* list;
  if (piece.color == uint8_t(PieceColor::WHITE)) {
    list = &white;
  }
  else {
    list = &black;
  }
  list->insert(list->end(), GetPiece(x, y));
  return true;
}

bool ChessBoard::RemovePiece(unsigned x, unsigned y)
{
  Piece* p = GetPiece(x, y);
  if (p->type == uint8_t(PieceType::EMPTY)) {
    return false;
  }
  PieceColor c = PieceColor(p->color);
  std::list<Piece*>* list;
  if (c == PieceColor::WHITE) {
    list = &white;
  }
  else {
    list = &black;
  }
  Vec2u8 coord(x, y);
  for (auto it = list->begin(); it != list->end(); it++) {
    Piece* ptr = (*it);
    if (ptr->pos == coord){
      list->erase(it);
      break;
    }
  }

  p->type = uint8_t(PieceType::EMPTY);
  return true;
}
