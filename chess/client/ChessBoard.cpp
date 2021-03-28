#include "ChessBoard.h"

ChessBoard::ChessBoard():nextColor(PieceColor::WHITE),
castleBk(true),
castleBQ(true),
castleWK(true),
castleWQ(true),
hasEnPassant(false),
enPassantDst(0,0)
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
  return fen;
}

