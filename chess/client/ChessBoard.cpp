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

void Char2Piece(char c, Piece & p) {
  const char LOWER_DIFF = 'a' - 'A';
  if (c > 'a') {
    p.color = uint8_t(PieceColor::BLACK);
    c -= LOWER_DIFF;
  }
  else {
    p.color = uint8_t(PieceColor::WHITE);
  }
  switch (c) {
  case 'P':
    p.type = uint8_t(PieceType::PAWN);
    break;
  case 'R':
    p.type = uint8_t(PieceType::ROOK);
    break;
  case  'N':
    p.type = uint8_t(PieceType::KNIGHT);
    break;
  case 'B':
    p.type = uint8_t(PieceType::BISHOP);
    break;
  case 'Q':
    p.type = uint8_t(PieceType::QUEEN);
    break;
  case 'K':
    p.type = uint8_t(PieceType::KING);
    break;
  default:
    //unknown shouldn't really get here ever.
    break;
  }
}

std::string coordString(const Vec2u8& coord)
{
  std::string s="  ";
  s[0] = 'a' + coord[0];
  s[1] = '1' + coord[1];
  return s;
}

int parseInt(const std::string& str, size_t & idx) 
{
  char c = str[idx];
  int num = 0;
  while (c != ' ' && idx<str.size()) {
    c -= '0';
    num *= 10;
    num += c;
    idx++;
    c = str[idx];
  }
  return num;
}

int ChessBoard::FromFen(const std::string& fen)
{
  size_t strIdx = 0;
  Clear();
  for (int row = BOARD_SIZE - 1; row >= 0; row--) {
    int emptyCnt = 0;
    int col = 0;
    while (col < BOARD_SIZE) {
      char c = fen[strIdx];
      if (c >= '1' && c <= '8') {
        emptyCnt = c - '0';
        col += emptyCnt;
        strIdx++;
      }
      else {
        Piece p;
        Char2Piece(c, p);
        AddPiece(col, row, p);
        col++;
        strIdx++;
      }
    }

    if (row > 0) {
      //go past '/'
      strIdx++;
    }
  }

  //space
  strIdx++;
  char c = fen[strIdx];
  if (c == 'w') {
    nextColor = PieceColor::WHITE;
  }
  else {
    nextColor = PieceColor::BLACK;
  }
  strIdx += 2;

  c = fen[strIdx];
  castleWK = false;
  castleWQ = false;
  castleBK = false;
  castleBQ = false;
  while (c != ' ' && strIdx<fen.size()) {
    if (c == '-') {
      break;
    }
    else if (c == 'K') {
      castleWK = true;
      strIdx++;
    }
    else if (c == 'Q') {
      castleWQ = true;
      strIdx++;
    }
    else if (c == 'k') {
      castleBK = true;
      strIdx++;
    }
    else if (c == 'q') {
      castleBK = true;
      strIdx++;
    }
    c = fen[strIdx];
  }
  strIdx++;
  c = fen[strIdx];
  if (c == '-') {
    hasEnPassant = false;
  }
  else {
    enPassantDst[0] = c - 'a';
    strIdx++;
    c = fen[strIdx];
    enPassantDst[1] = c - '1';
  }
 
  strIdx+=2;
  halfMoves = parseInt(fen, strIdx);
  strIdx++;
  fullMoves = parseInt(fen, strIdx);
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

void ChessBoard::Clear()
{
  black.clear();
  white.clear();
  for (size_t i = 0; i < board.size(); i++) {
    board[i].type = uint8_t(PieceType::EMPTY);
  }

  hasEnPassant = false;
  fullMoves = 0;
  halfMoves = 0;

  castleBK = true;
  castleBQ = true;
  castleWK = true;
  castleWQ = true;

}
