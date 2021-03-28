#include "ChessBoard.h"

#include <sstream>

ChessBoard::ChessBoard() :nextColor(PieceColor::WHITE),
castleBk(true),
castleBQ(true),
castleWK(true),
castleWQ(true),
hasEnPassant(false),
enPassantDst(0, 0)
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

int ChessBoard::FromFen(const std::string& fen)
{
  return 0;
}

std::string ChessBoard::GetFen()
{
  std::string fen;
  std::ostringstream oss;
  for (unsigned row = 0; row < BOARD_SIZE; row++) {
    for (unsigned col = 0; col < BOARD_SIZE; col++){

    }
  }
  return fen;
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
