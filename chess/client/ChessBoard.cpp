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

int ChessBoard::numChecks() {
	return 0;
}

void ChessBoard::GetKingEvasions(std::vector<Move>& moves) {

}

void ChessBoard::GetCaptures(std::vector<Move>& moves) {

}

void ChessBoard::GetQuiets(std::vector<Move>& moves)
{
  if (nextColor == PieceColor::BLACK) {
    for (const Piece* p : black) {
      GetQuiets(moves, *p);
    }
  }
  else if (nextColor == PieceColor::WHITE) {
    for (const Piece* p : white) {
      GetQuiets(moves, *p);
    }
  }
}

void ChessBoard::GetQuiets(std::vector<Move>& moves, const Piece& p)
{
  uint8_t t = p.type();
  PieceType type = PieceType(t);
  switch (type) {
  case PieceType::PAWN:
    GetQuietsPawn(moves, p);
    break;
  case PieceType::ROOK:

    break;
  case PieceType::KNIGHT:

    break;
  case PieceType::BISHOP:

    break;
  case PieceType::QUEEN:

    break;
  case PieceType::KING:

    break;
  }
}

void ChessBoard::GetQuietsPawn(std::vector<Move>& moves, const Piece& p)
{
  PieceColor color = PieceColor(p.color());
  uint8_t row = p.pos.Row();
  uint8_t col = p.pos.Col();
  if (color == PieceColor::BLACK) {
    if (row == 0) {
      //shouldn't happen because it should have been promoted.
      return;
    }
    if (row == 7) {

    }
    ChessCoord dst(col - 1, row);
    Move m(p.pos, dst);
    if (col == 2) {

    }
    else {
      moves.push_back(m);
    }
  }
  else {
    if (row == 7) {
      return;
    }
  }
}

void ChessBoard::GetEvasions(std::vector<Move>& moves) {
	GetKingEvasions(moves);

	if (numChecks() > 1) return; // If multiple pieces are checking the king, he has to move

	// Generate blocks/captures to get out of check
}

void ChessBoard::GetNonEvasions(std::vector<Move>& moves) {
	GetCaptures(moves);
	GetQuiets(moves);
}


std::vector<Move> ChessBoard::GetMoves()
{
  std::vector<Move> moves;

  if (numChecks() > 0) {
	  GetEvasions(moves);
  }
  else {
	  GetNonEvasions(moves);
  }

  return moves;
}

int ChessBoard::ApplyMove(const Move& m)
{

  return 0;
}

char PieceFEN(const PieceInfo& info)
{
  PieceType type = PieceType(info.type());
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

  if (info.color() == uint8_t(PieceColor::BLACK)) {
    c += LOWER_DIFF;
  }
  return c;
}

PieceInfo Char2PieceInfo(char c)
{
  PieceInfo info(0);
  const char LOWER_DIFF = 'a' - 'A';
  if (c > 'a') {
    info.SetColor(PieceColor::BLACK);
    c -= LOWER_DIFF;
  }
  else {
    info.SetColor(PieceColor::WHITE);
  }
  switch (c) {
  case 'P':
    info.SetType(PieceType::PAWN);
    break;
  case 'R':
    info.SetType(PieceType::ROOK);
    break;
  case  'N':
    info.SetType(PieceType::KNIGHT);
    break;
  case 'B':
    info.SetType(PieceType::BISHOP);
    break;
  case 'Q':
    info.SetType(PieceType::QUEEN);
    break;
  case 'K':
    info.SetType(PieceType::KING);
    break;
  default:
    //unknown shouldn't really get here ever.
    break;
  }
  return info;
}

std::string coordString(const ChessCoord& coord)
{
  std::string s="  ";
  s[0] = 'a' + coord.Col();
  s[1] = '1' + coord.Row();
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

void ChessBoard::SetStartPos()
{
  const std::string startFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  FromFen(startFen);
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
        p.info = Char2PieceInfo(c);
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
    uint8_t col = c - 'a';
    strIdx++;
    c = fen[strIdx];
    uint8_t row = c - '1';
    enPassantDst.Set(col, row);
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
      if (p.type() == uint8_t(PieceType::EMPTY)) {
        emptyCnt++;
      }
      else {
        if (emptyCnt > 0) {
          oss << emptyCnt;
        }
        char c = PieceFEN(p.info);
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
  if ((*this)(x, y).type() != uint8_t(PieceType::EMPTY)) {
    return false;
  }
  (*this)(x, y) = piece;
  (*this)(x, y).pos = ChessCoord(x, y);

  std::vector<Piece*>* list;
  if (piece.color() == uint8_t(PieceColor::WHITE)) {
    list = &white;
  }
  else {
    list = &black;
  }
  list->push_back(GetPiece(x, y));
  return true;
}

bool ChessBoard::RemovePiece(unsigned x, unsigned y)
{
  Piece* p = GetPiece(x, y);
  if (p->type() == uint8_t(PieceType::EMPTY)) {
    return false;
  }
  PieceColor c = PieceColor(p->color());
  std::vector<Piece*>* list;
  if (c == PieceColor::WHITE) {
    list = &white;
  }
  else {
    list = &black;
  }
  ChessCoord coord(x, y);
  for (auto it = list->begin(); it != list->end(); it++) {
    Piece* ptr = (*it);
    if (ptr->pos == coord){
      list->erase(it);
      break;
    }
  }

  p->SetType(PieceType::EMPTY);
  return true;
}

void ChessBoard::Clear()
{
  black.clear();
  white.clear();
  for (size_t i = 0; i < board.size(); i++) {
    board[i].SetType (PieceType::EMPTY);
  }

  hasEnPassant = false;
  fullMoves = 0;
  halfMoves = 0;

  castleBK = true;
  castleBQ = true;
  castleWK = true;
  castleWQ = true;

}
