#include "ChessBoard.h"

#include <sstream>

#define a1 0
#define c1 2
#define d1 3
#define e1 4
#define f1 5
#define g1 6
#define h1 7
#define a8 56
#define c8 58
#define d8 59
#define e8 60
#define f8 61
#define g8 62
#define h8 63

#define BLACK_ROOK (2)
#define BLACK_KNIGHT (3)
#define BLACK_BISHOP (4)
#define BLACK_QUEEN (5)
#define WHITE_ROOK (10)
#define WHITE_KNIGHT (11)
#define WHITE_BISHOP (12)
#define WHITE_QUEEN (13)

enum CastleMove {
  CASTLE_NONE=0,
  CASTLE_BQ,
  CASTLE_BK,
  CASTLE_WQ,
  CASTLE_WK
};

void AddWhitePromos(const Move& m_in, std::vector<Move>& moves);
void AddBlackPromos(const Move& m_in, std::vector<Move>& moves);

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
  for (size_t i = 0; i < board.size(); i++) {
    board[i].pos = uint8_t(i);
  }
}

int ChessBoard::numChecks() {
	return 0;
}

void ChessBoard::GetKingEvasions(std::vector<Move>& moves) {

}

void ChessBoard::GetCaptures(std::vector<Move>& moves)
{
  if (nextColor == PieceColor::BLACK) {
    for (const Piece* p : black) {
      GetCaptures(moves, *p);
    }
  }
  else if (nextColor == PieceColor::WHITE) {
    for (const Piece* p : white) {
      GetCaptures(moves, *p);
    }
  }
}

void ChessBoard::GetCaptures(std::vector<Move>& moves, const Piece& p)
{
  PieceType type = PieceType(p.type());
  switch (type) {
  case PieceType::PAWN:
    GetCapturesPawn(moves, p);
    break;
  case PieceType::ROOK:
    GetCapturesRook(moves, p);
    break;
  case PieceType::KNIGHT:
    GetCapturesKnight(moves, p);
    break;
  case PieceType::BISHOP:
    GetCapturesBishop(moves, p);
    break;
  case PieceType::QUEEN:

    break;
  case PieceType::KING:

    break;
  }
}

void ChessBoard::AddBlackPawnCaptures(ChessCoord src, ChessCoord dst, std::vector<Move>& moves)
{
  Piece* dstPiece = GetPiece(dst);
  bool hasWhitePiece = (!dstPiece->isEmpty()) 
    && (dstPiece->color() == uint8_t(PieceColor::WHITE));
  if (hasWhitePiece || 
    (src.Row() == 3 && enPassantDst == dst) ) {
    Move m(src, dst);
    if (src.Row() == 1) {
      AddBlackPromos(m, moves);
    }
    else {
      moves.push_back(m);
    }
  }
}

void ChessBoard::AddWhitePawnCaptures(ChessCoord src, ChessCoord dst, std::vector<Move>& moves)
{
  Piece* dstPiece = GetPiece(dst);
  bool hasBlackPiece = (!dstPiece->isEmpty())
    && (dstPiece->color() == uint8_t(PieceColor::BLACK));

  if (hasBlackPiece ||
    (src.Row() == 4 && enPassantDst == dst)) {
    Move m(src, dst);
    if (src.Row() == 6) {
      AddWhitePromos(m, moves);
    }
    else {
      moves.push_back(m);
    }
  }
}

void ChessBoard::GetCapturesPawn(std::vector<Move>& moves, const Piece& p)
{
  PieceColor color = PieceColor(p.color());
  uint8_t row = p.pos.Row();
  uint8_t col = p.pos.Col();
  if (color == PieceColor::BLACK) {
    if (row == 0) {
      //shouldn't happen because it should have been promoted.
      return;
    }
    ChessCoord dst = p.pos;
    if (col > 0) {
      dst.DecRow();
      dst.DecCol();
      AddBlackPawnCaptures(p.pos, dst, moves);
    }
    if (col < 7) {
      dst = p.pos;
      dst.DecRow();
      dst.IncCol();
      AddBlackPawnCaptures(p.pos, dst, moves);
    }
  }
  else {
    if (row == 7) {
      return;
    }
    ChessCoord dst = p.pos;
    if (col > 0) {
      dst.IncRow();
      dst.DecCol();
      AddWhitePawnCaptures(p.pos, dst, moves);
    }
    if (col < 7) {
      dst = p.pos;
      dst.IncRow();
      dst.IncCol();
      AddWhitePawnCaptures(p.pos, dst, moves);
    }
  }
}

void ChessBoard::GetCapturesRook(std::vector<Move>& moves, const Piece& p)
{
  uint8_t color = p.color();
  char r0 = char (p.pos.Row());
  char c0 = char (p.pos.Col());
  const unsigned NUM_DIRS = 4;
  char dir[NUM_DIRS][2] = { {-1,0},{1,0},{0,-1},{0,1} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    char row = r0;
    char col = c0;
    for (char step = 0; step < BOARD_SIZE; step++) {
      col += dir[d][0];
      if (col < 0 || col >= BOARD_SIZE) {
        break;
      }
      row += dir[d][1];
      if (row < 0 || row >= BOARD_SIZE) {
        break;
      }
      Piece * dstPiece = GetPiece(col, row);
      if (!dstPiece->isEmpty()) {
        if (dstPiece->color() != color) {
          Move m(p.pos, ChessCoord(col, row));
          moves.push_back(m);
        }
        break;
      }
    }
  }
}

void ChessBoard::GetCapturesKnight(std::vector<Move>& moves, const Piece& p)
{
  uint8_t color = p.color();
  char r0 = char(p.pos.Row());
  char c0 = char(p.pos.Col());
  const unsigned NUM_DIRS = 8;
  char dir[NUM_DIRS][2] = { {-2,-1},{-2,1},{-1,2},{1,2},
    {2,1},{2,-1},{1,-2},{-1,-2} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    char col = c0 + dir[d][0];
    char row = r0 + dir[d][1];
    if (col < 0 || col >= BOARD_SIZE) {
      continue;
    }
    if (row < 0 || row >= BOARD_SIZE) {
      continue;
    }
    Piece* dstPiece = GetPiece(col, row);
    if (!dstPiece->isEmpty()) {
      if (dstPiece->color() != color) {
        Move m(p.pos, ChessCoord(col, row));
        moves.push_back(m);
      }      
    }
  }
}

void ChessBoard::GetCapturesBishop(std::vector<Move>& moves, const Piece& p)
{
  uint8_t color = p.color();
  char r0 = char(p.pos.Row());
  char c0 = char(p.pos.Col());
  const unsigned NUM_DIRS = 4;
  char dir[NUM_DIRS][2] = { {-1,-1},{-1,1},{1,-1},{1,1} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    char row = r0;
    char col = c0;
    for (char step = 0; step < BOARD_SIZE; step++) {
      col += dir[d][0];
      if (col < 0 || col >= BOARD_SIZE) {
        break;
      }
      row += dir[d][1];
      if (row < 0 || row >= BOARD_SIZE) {
        break;
      }
      Piece* dstPiece = GetPiece(col, row);
      if (!dstPiece->isEmpty()) {
        if (dstPiece->color() != color) {
          Move m(p.pos, ChessCoord(col, row));
          moves.push_back(m);
        }
        break;
      }
    }
  }
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
  PieceType type = PieceType(p.type());
  switch (type) {
  case PieceType::PAWN:
    GetQuietsPawn(moves, p);
    break;
  case PieceType::ROOK:
    GetQuietsRook(moves, p);
    break;
  case PieceType::KNIGHT:
    GetQuietsKnight(moves, p);
    break;
  case PieceType::BISHOP:
    GetQuietsBishop(moves, p);
    break;
  case PieceType::QUEEN:

    break;
  case PieceType::KING:

    break;
  }
}

void AddBlackPromos(const Move & m_in, std::vector<Move> & moves) {
  Move m = m_in;
  m.promo = BLACK_ROOK;
  moves.push_back(m);
  m.promo = BLACK_KNIGHT;
  moves.push_back(m);
  m.promo = BLACK_BISHOP;
  moves.push_back(m);
  m.promo = BLACK_QUEEN;
  moves.push_back(m);
}

void AddWhitePromos(const Move& m_in, std::vector<Move>& moves) {
  Move m = m_in;
  m.promo = WHITE_ROOK;
  moves.push_back(m);
  m.promo = WHITE_KNIGHT;
  moves.push_back(m);
  m.promo = WHITE_BISHOP;
  moves.push_back(m);
  m.promo = WHITE_QUEEN;
  moves.push_back(m);
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
    ChessCoord dst = p.pos;
    dst.DecRow();
    if (GetPiece(dst)->isEmpty()) {
      Move m(p.pos, dst);
      if (row == 1) {
        AddBlackPromos(m, moves);
      }
      else {
        moves.push_back(m);
        if (row == 6) {
          dst.DecRow();
          if (GetPiece(dst)->isEmpty()) {
            m.dst = dst;
            moves.push_back(m);
          }
        }
      }     
    }
  }
  else {
    if (row == 7) {
      return;
    }
    ChessCoord dst = p.pos;
    dst.IncRow();
    if (GetPiece(dst)->isEmpty()) {
      Move m(p.pos, dst);
      if (row == 6) {
        AddWhitePromos(m, moves);
      }
      else {
        moves.push_back(m);
        if (row == 1) {
          dst.IncRow();
          if (GetPiece(dst)->isEmpty()) {
            m.dst = dst;
            moves.push_back(m);
          }
        }
      }
    }
  }
}

void ChessBoard::GetQuietsRook(std::vector<Move>& moves, const Piece& p)
{
  uint8_t color = p.color();
  char r0 = char(p.pos.Row());
  char c0 = char(p.pos.Col());
  const unsigned NUM_DIRS = 4;
  char dir[NUM_DIRS][2] = { {-1,0},{1,0},{0,-1},{0,1} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    char row = r0;
    char col = c0;
    for (char step = 0; step < BOARD_SIZE; step++) {
      col += dir[d][0];
      if (col < 0 || col >= BOARD_SIZE) {
        break;
      }
      row += dir[d][1];
      if (row < 0 || row >= BOARD_SIZE) {
        break;
      }
      Piece* dstPiece = GetPiece(col, row);
      if (dstPiece->isEmpty()) {
        Move m(p.pos, ChessCoord(col, row));
        moves.push_back(m);
        continue;
      }
      else {
        break;
      }
    }
  }
}

void ChessBoard::GetQuietsKnight(std::vector<Move>& moves, const Piece& p)
{
  uint8_t color = p.color();
  char r0 = char(p.pos.Row());
  char c0 = char(p.pos.Col());
  const unsigned NUM_DIRS = 8;
  char dir[NUM_DIRS][2] = { {-2,-1},{-2,1},{-1,2},{1,2},
    {2,1},{2,-1},{1,-2},{-1,-2} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    char col = c0 + dir[d][0];
    char row = r0 + dir[d][1];
    if (col < 0 || col >= BOARD_SIZE) {
      continue;
    }
    if (row < 0 || row >= BOARD_SIZE) {
      continue;
    }
    Piece* dstPiece = GetPiece(col, row);
    if (dstPiece->isEmpty()) {
      Move m(p.pos, ChessCoord(col, row));
      moves.push_back(m);
    }
  }
}

void ChessBoard::GetQuietsBishop(std::vector<Move>& moves, const Piece& p) {
  uint8_t color = p.color();
  char r0 = char(p.pos.Row());
  char c0 = char(p.pos.Col());
  const unsigned NUM_DIRS = 4;
  char dir[NUM_DIRS][2] = { {-1,-1},{-1,1},{1,-1},{1,1} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    char row = r0;
    char col = c0;
    for (char step = 0; step < BOARD_SIZE; step++) {
      col += dir[d][0];
      if (col < 0 || col >= BOARD_SIZE) {
        break;
      }
      row += dir[d][1];
      if (row < 0 || row >= BOARD_SIZE) {
        break;
      }
      Piece* dstPiece = GetPiece(col, row);
      if (dstPiece->isEmpty()) {
        Move m(p.pos, ChessCoord(col, row));
        moves.push_back(m);
      }
      else {
        break;
      }
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
  Piece* p = GetPiece(m.src);
  uint8_t color = p->color();
  if (m.src == m.dst) {
    return -1;
  }
  if (color == uint8_t(nextColor)) {
    nextColor = PieceColor(!color);
  }
  else {
    return -2;
  }

  PieceType type = PieceType(p->type());
  hasEnPassant = false;
  enPassantDst = ChessCoord();
  halfMoves++;
  CastleMove castling = CASTLE_NONE;
  //is this move en Passant
  bool isEnPassant=false;
  if (color == uint8_t(PieceColor::BLACK) ){
    switch (type) {
    case PieceType::PAWN:
      if (m.src.Row() == 6 && m.dst.Row() == 4) {
        enPassantDst = ChessCoord(m.dst.Col(), 5);
        hasEnPassant = true;
      }
      if (m.src.Col() != m.dst.Col() && GetPiece(m.dst)->isEmpty()) {
        isEnPassant = true;
      }
      halfMoves = 0;
      break;
    case PieceType::KING:
      if (m.src == e8) {
        if (castleBQ && m.dst == c8) {
          castling = CASTLE_BQ;
        }
        else if (castleBK && m.dst == g8) {
          castling = CASTLE_BK;
        }
      }
      castleBK = false;
      castleBQ = false;
      break;
    case PieceType::ROOK:
      if (m.src.Col() == 0) {
        castleBQ = false;
      }
      else if (m.src.Col() == 7) {
        castleBK = false;
      }
      break;
    }

    fullMoves++;
  }
  else {
    switch (type) {
    case PieceType::PAWN:
      if (m.src.Row() == 1 && m.dst.Row() == 3) {
        enPassantDst = ChessCoord(m.dst.Col(), 2);
        hasEnPassant = true;
      }
      if (m.src.Col() != m.dst.Col() && GetPiece(m.dst)->isEmpty()) {
        isEnPassant = true;
      }
      halfMoves = 0;
      break;
    case PieceType::KING:
      if (m.src == e1) {
        if (castleWQ && m.dst == c1) {
          castling = CASTLE_WQ;
        }
        else if (castleWK && m.dst == g1) {
          castling = CASTLE_WK;
        }
      }
      castleWK = false;
      castleWQ = false;
      break;
    case PieceType::ROOK:
      if (m.src.Col() == 0) {
        castleWQ = false;
      }
      else if (m.src.Col() == 7) {
        castleWK = false;
      }
      break;
    }
  }

  MovePiece(m.src, m.dst);
  Piece* dstp = GetPiece(m.dst);
  if (!m.promo.isEmpty()) {
    dstp->info = m.promo;
  }
  if (isEnPassant) {
    if (m.dst.Row() == 5) {
      RemovePiece(ChessCoord(m.dst.Col(), m.dst.Row() - 1));
    }
    else if (m.dst.Row() == 2) {
      RemovePiece(ChessCoord(m.dst.Col(), m.dst.Row() + 1));
    }
  }
  switch (castling) {
    //move the rook
  case CASTLE_BQ:
    MovePiece(d8, a8);
    break;
  case CASTLE_BK:
    MovePiece(f8, h8);
    break;
  case CASTLE_WQ:
    MovePiece(d1, a1);
    break;
  case CASTLE_WK:
    MovePiece(f1, h1);
    break;
  }
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
        PieceInfo info = Char2PieceInfo(c);
        AddPiece(ChessCoord(col, row), info);
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
      castleBQ = true;
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

bool ChessBoard::AddPiece(ChessCoord c, PieceInfo info)
{
  Piece* p = GetPiece(c);
  if (!p->isEmpty()) {
    return false;
  }
  p->info = info;

  std::vector<Piece*>* list;
  if (p->color() == uint8_t(PieceColor::WHITE)) {
    list = &white;
  }
  else {
    list = &black;
  }
  list->push_back(p);
  return true;
}

bool ChessBoard::RemovePiece(ChessCoord c)
{
  Piece* p = GetPiece(c);
  if (p->isEmpty()) {
    return false;
  }
  std::vector<Piece*>* list;
  if (p->color() == uint8_t(PieceColor::WHITE) ){
    list = &white;
  }
  else {
    list = &black;
  }
  for (auto it = list->begin(); it != list->end(); it++) {
    Piece* ptr = (*it);
    if (ptr->pos == c){
      list->erase(it);
      break;
    }
  }
  p->clear();
  return true;
}

///moves a piece from src to dst and update the piece lists.
///does not care rules. does nothing if src is empty.
bool ChessBoard::MovePiece(ChessCoord src, ChessCoord dst)
{
  Piece* p = GetPiece(src);
  if (p->isEmpty()) {
    return false;
  }
  RemovePiece(dst);

  std::vector<Piece*>* list;
  if (p->color() == uint8_t(PieceColor::WHITE)) {
    list = &white;
  }
  else {
    list = &black;
  }
  for (size_t i = 0; i < list->size(); i++) {
    Piece* ptr = (*list)[i];
    if (ptr->pos == src) {
      Piece* dstPiece = GetPiece(dst);
      (*list)[i] = dstPiece;
      dstPiece->info = p->info;
      p->clear();
      break;
    }
  }
}

void ChessBoard::Clear()
{
  black.clear();
  white.clear();
  for (size_t i = 0; i < board.size(); i++) {
    board[i].clear();
  }

  hasEnPassant = false;
  fullMoves = 0;
  halfMoves = 0;

  castleBK = true;
  castleBQ = true;
  castleWK = true;
  castleWQ = true;

}
