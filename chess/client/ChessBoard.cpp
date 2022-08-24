#include "ChessBoard.h"

#include <algorithm>
#include <sstream>

#define a1 0
#define b1 1
#define c1 2
#define d1 3
#define e1 4
#define f1 5
#define g1 6
#define h1 7
#define a3 16
#define h3 23
#define a4 24
#define h4 31
#define a5 32
#define a6 40
#define h5 39
#define h6 47
#define a8 56
#define b8 57
#define c8 58
#define d8 59
#define e8 60
#define f8 61
#define g8 62
#define h8 63

#define BLACK_PAWN (1)
#define BLACK_ROOK (2)
#define BLACK_KNIGHT (3)
#define BLACK_BISHOP (4)
#define BLACK_QUEEN (5)
#define WHITE_PAWN (9)
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

ChessBoard::ChessBoard() :nextColor(PIECE_WHITE),
castleBK(true),
castleBQ(true),
castleWK(true),
castleWQ(true),
hasEnPassant(false),
enPassantDst(0, 0),
halfMoves(0),
fullMoves(0)
{
  hasCastled[0] = false;
  hasCastled[1] = false;
  board.resize(64);
}

std::vector<Move> ChessBoard::GetMoves()
{
  std::vector<Move> moves;
  checksInfo = ComputeChecks();
  //std::cout << "Debug:\n" << checksInfo.ToString() << "\n";
  if (checksInfo.attackers.size() > 0) {
    GetEvasions(moves);
  }
  else {
    GetNonEvasions(moves);
  }

  return moves;
}

void ChessBoard::GetEvasions(std::vector<Move>& moves)
{
  GetKingEvasions(moves);

  // If multiple pieces are checking the king, he has to move
  if (checksInfo.attackers.size() > 1) { return; }

  // Generate blocks/captures to get out of check
  GetBlockingMoves(moves);
}

void ChessBoard::GetNonEvasions(std::vector<Move>& moves) {
  GetCaptures(moves);
  GetQuiets(moves);
}

void ChessBoard::GetKingEvasions(std::vector<Move>& moves)
{
  ChessCoord kingCoord = checksInfo.kingCoord;

  const unsigned NUM_DIRS = 8;
  char dir[NUM_DIRS][2] = { {-1,0},{1,0},{0,-1},{0,1},
    {-1,-1},{-1,1},{1,-1},{1,1} };
  char row = kingCoord.Row();
  char col = kingCoord.Col();
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    char dstCol = col + dir[d][0];
    char dstRow = row + dir[d][1];
    if (dstCol < 0 || dstCol>7 || dstRow < 0 || dstRow>7) {
      continue;
    }
    ChessCoord dst(dstCol, dstRow);
    bool attacked = checksInfo.attacked.GetBit(dst.coord);
    if (attacked) {
      continue;
    }
    Piece* dstPiece = GetPiece(dst);
    bool available = (dstPiece->isEmpty() || dstPiece->color() != uint8_t(nextColor));
    if (available) {
      moves.push_back(Move(kingCoord, dst));
    }  
  }
}

void AddBlockingMoves(ChessCoord src, const std::vector<ChessCoord> & dsts,
  BitBoard targetSquares, std::vector<Move> & moves)
{
  for (ChessCoord dst : dsts) {
    if (targetSquares.GetBit(dst)) {
      moves.push_back(Move(src, dst));
    }
  }
}

void ChessBoard::GetBlockingMoves(std::vector<Move>& moves)
{
  ChessCoord kingCoord = checksInfo.kingCoord;
  //there should be only 1 attacker.
  ChessCoord attacker = checksInfo.attackers[0];
  uint8_t attackerType = GetPiece(attacker)->type();
  //need to move to any of the target squares.
  BitBoard targetSquares;
  //one option is to capture the attacker
  targetSquares.SetBit(attacker);
  //en passant is a bitch
  char row = attacker.Row();
  char col = attacker.Col();
  bool canUseEnPassant = false;
  if (hasEnPassant && attackerType == uint8_t(PIECE_PAWN)
    && col == char(enPassantDst.Col()) ) {
    if (nextColor == PIECE_BLACK) {
      if (row == char(enPassantDst.Row() + 1)) {
        canUseEnPassant = true;
      }
    }
    else {
      if (row == char(enPassantDst.Row() - 1)) {
        canUseEnPassant = true;
      }
    }
  }

  if ( (attackerType == PIECE_ROOK)
    || (attackerType == PIECE_BISHOP)
    || (attackerType == PIECE_QUEEN) ) {
    char xDist = char(kingCoord.Col()) - char(col);
    char yDist = char(kingCoord.Row()) - char(row);
    char dx = 0, dy = 0;
    if (xDist > 0) {
      dx = 1;
    } else if (xDist < 0) {
      dx = -1;
    }
    if (yDist > 0) {
      dy = 1;
    }
    else if (yDist < 0) {
      dy = -1;
    }
    char numSteps = 0;
    if (dx != 0) {
      numSteps = xDist / dx - 1;
    }
    else {
      //dx and dy can't both be 0
      numSteps = yDist / dy - 1;
    }

    for (char step = 0; step < numSteps; step++) {
      col += dx;
      row += dy;
      ChessCoord dst=ChessCoord(uint8_t(col), uint8_t(row));
      targetSquares.SetBit(dst);
    }
  }
  //else can't block knight and pawn. king can't give checks.

  std::vector<ChessCoord>* list = GetPieceList(nextColor);
  for (ChessCoord c : (*list)) {
    //when in check, cannot move pinned pieces.
    if (checksInfo.blockers.GetBit(c)) {
      continue;
    }
    //can't use king to block
    if (c == checksInfo.kingCoord) {
      continue;
    }
    uint8_t type = GetPiece(c)->type();
    std::vector<ChessCoord> dstCoords;
    switch (type) {
    case PIECE_PAWN:
      //pawn is a pain in the ass
      dstCoords = GetBlockingPawn(c, canUseEnPassant);
      for (ChessCoord dst : dstCoords) {
        if (targetSquares.GetBit(dst)) {
          Move m(c, dst);
          if (nextColor == PIECE_BLACK && row==1) {
            AddBlackPromos(m, moves);
          }
          else if (nextColor == PIECE_WHITE && row == 6) {
            AddWhitePromos(m, moves);
          }
          else {
            moves.push_back(m);
          }
        }
      }
      if (canUseEnPassant) {
        for (ChessCoord dst : dstCoords) {
          if (dst == enPassantDst) {
            moves.push_back(Move(c, dst));
          }
        }
      }
      break;
    case PIECE_ROOK:
      dstCoords = GetDstRook(c);
      AddBlockingMoves(c, dstCoords, targetSquares, moves);
      break;
    case PIECE_KNIGHT:
      dstCoords = GetDstKnight(c);
      AddBlockingMoves(c, dstCoords, targetSquares, moves);
      break;
    case PIECE_BISHOP:
      dstCoords = GetDstBishop(c);
      AddBlockingMoves(c, dstCoords, targetSquares, moves);
      break;
    case PIECE_QUEEN:
      dstCoords = GetDstQueen(c);
      AddBlockingMoves(c, dstCoords, targetSquares, moves);
      break;
    }
  }
}

std::vector<ChessCoord> ChessBoard::GetBlockingPawn(ChessCoord c, 
  bool canUseEnPassant)
{
  std::vector<ChessCoord> coords;
  uint8_t row = c.Row();
  uint8_t col = c.Col();
  //pawn can block by quiets or captures including en passant
  if (nextColor == PIECE_BLACK) {
    ChessCoord dst = ChessCoord(col, row - 1);
    if (GetPiece(dst)->isEmpty()) {
      coords.push_back(dst);
      if (row == 6) {
        dst.coord -= 8;
        if (GetPiece(dst)->isEmpty()) {
          coords.push_back(dst);
        }
      }
    }

    if (col > 0) {
      dst = ChessCoord(col - 1, row - 1);
      Piece* dstPiece = GetPiece(dst);
      if (!dstPiece->isEmpty() && dstPiece->color() == PIECE_WHITE) {
        coords.push_back(dst);
      }
    }
    if (col < 7) {
      dst = ChessCoord(col + 1, row - 1);
      Piece* dstPiece = GetPiece(dst);
      if (!dstPiece->isEmpty() && dstPiece->color() == PIECE_WHITE) {
        coords.push_back(dst);
      }
    }

    if (canUseEnPassant && row == 3) {
      if (col > 0) {
        dst = ChessCoord(col - 1, row - 1);
        if (hasEnPassant && dst == enPassantDst) {
          coords.push_back(dst);
        }
      }
      if (col < 7) {
        dst = ChessCoord(col + 1, row - 1);
        if (hasEnPassant && dst == enPassantDst) {
          coords.push_back(dst);
        }
      }
    }
  }
  else {
    ChessCoord dst = ChessCoord(col, row + 1);
    if (GetPiece(dst)->isEmpty()) {
      coords.push_back(dst);
      if (row == 1) {
        dst.coord += 8;
        if (GetPiece(dst)->isEmpty()) {
          coords.push_back(dst);
        }
      }
    }

    if (col > 0) {
      dst = ChessCoord(col - 1, row + 1);
      Piece* dstPiece = GetPiece(dst);
      if (!dstPiece->isEmpty() && dstPiece->color() == PIECE_BLACK) {
        coords.push_back(dst);
      }
    }
    if (col < 7) {
      dst = ChessCoord(col + 1, row + 1);
      Piece* dstPiece = GetPiece(dst);
      if (!dstPiece->isEmpty() && dstPiece->color() == PIECE_BLACK) {
        coords.push_back(dst);
      }
    }

    if (canUseEnPassant && row == 4) {
      if (col > 0) {
        dst = ChessCoord(col - 1, row + 1);
        if (hasEnPassant && dst == enPassantDst) {
          coords.push_back(dst);
        }
      }
      if (col < 7) {
        dst = ChessCoord(col + 1, row + 1);
        if (hasEnPassant && dst == enPassantDst) {
          coords.push_back(dst);
        }
      }
    }
  }
  return coords;
}

std::vector<ChessCoord> ChessBoard::GetDstRook(ChessCoord c)
{
  std::vector<ChessCoord> coords;
  uint8_t color = GetPiece(c)->color();
  //rook can block by quiets
  char r0 = char(c.Row());
  char c0 = char(c.Col());
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
        coords.push_back(ChessCoord(col, row));
      }
      else {
        if (dstPiece->color() != color) {
          coords.push_back(ChessCoord(col, row));
        }
        break;
      }
    }
  }
  return coords;
}

std::vector<ChessCoord> ChessBoard::GetDstKnight(ChessCoord c)
{
  std::vector<ChessCoord> coords;
  uint8_t color = uint8_t(nextColor);
  char r0 = char(c.Row());
  char c0 = char(c.Col());
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
    if (dstPiece->isEmpty() || dstPiece->color() != color) {
      coords.push_back( ChessCoord(col, row));
    }
  }
  return coords;
}

std::vector<ChessCoord>ChessBoard::GetDstBishop(ChessCoord c)
{
  uint8_t color = GetPiece(c)->color();
  char r0 = char(c.Row());
  char c0 = char(c.Col());
  const unsigned NUM_DIRS = 4;
  char dir[NUM_DIRS][2] = { {-1,-1},{-1,1},{1,-1},{1,1} };
  std::vector<ChessCoord> coords;
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
          coords.push_back(ChessCoord(col, row));
        }
        break;
      }
      else {
        coords.push_back(ChessCoord(col, row));
      }
    }
  }
  return coords;
}

std::vector<ChessCoord>ChessBoard::GetDstQueen(ChessCoord c)
{
  uint8_t color = GetPiece(c)->color();
  char r0 = char(c.Row());
  char c0 = char(c.Col());
  const unsigned NUM_DIRS = 8;
  char dir[NUM_DIRS][2] = { {-1,0},{1,0},{0,-1},{0,1} , 
    {-1,-1},{-1,1},{1,-1},{1,1} };
  std::vector<ChessCoord> coords;
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
          coords.push_back(ChessCoord(col, row));
        }
        break;
      }
      else {
        coords.push_back(ChessCoord(col, row));
      }
    }
  }
  return coords;
}

std::vector<ChessCoord> * ChessBoard::GetPieceList(uint8_t c)
{
  std::vector<ChessCoord>* list = &(pieces[c]);
  return list;
}

void ChessBoard::GetCaptures(std::vector<Move>& moves)
{
  std::vector<ChessCoord>* list = GetPieceList(nextColor);
  for (ChessCoord c : (*list) ){
    ///@TODO
    //if pinned have to take the pinner
    std::vector<ChessCoord> dsts;
    GetCaptures(moves, c);
  }
}

void ChessBoard::GetCaptures(std::vector<Move>& moves, ChessCoord c)
{
  uint8_t type = GetPiece(c)->type();
  switch (type) {
  case PIECE_PAWN:
    GetCapturesPawn(moves, c);
    break;
  case PIECE_ROOK:
    GetCapturesRook(moves, c);
    break;
  case PIECE_KNIGHT:
    GetCapturesKnight(moves, c);
    break;
  case PIECE_BISHOP:
    GetCapturesBishop(moves, c);
    break;
  case PIECE_QUEEN:
    GetCapturesQueen(moves, c);
    break;
  case PIECE_KING:
    GetCapturesKing(moves, c);
    break;
  }
}

void ChessBoard::AddBlackPawnCaptures(ChessCoord src, ChessCoord dst, std::vector<Move>& moves)
{
  ChessCoord pinner;
  bool pinned = checksInfo.blockers.GetBit(src);
  if (pinned) {
    pinner = checksInfo.pinners[src.coord];
  }
  if (pinned && dst != pinner) {
    return;
  }

  Piece* dstPiece = GetPiece(dst);
  bool hasWhitePiece = (!dstPiece->isEmpty()) 
    && (dstPiece->isWhite());
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
  ChessCoord pinner;
  bool pinned = checksInfo.blockers.GetBit(src);
  if (pinned) {
    pinner = checksInfo.pinners[src.coord];
  }
  if (pinned && dst != pinner) {
    return;
  }

  Piece* dstPiece = GetPiece(dst);
  bool hasBlackPiece = (!dstPiece->isEmpty())
    && (dstPiece->isBlack());

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

void ChessBoard::GetCapturesPawn(std::vector<Move>& moves, ChessCoord c)
{
  uint8_t color = GetPiece(c)->color();
  uint8_t row = c.Row();
  uint8_t col = c.Col();
  if (color == PIECE_BLACK) {
    if (row == 0) {
      //shouldn't happen because it should have been promoted.
      return;
    }
    ChessCoord dst = c;
    if (col > 0) {
      dst.DecRow();
      dst.DecCol();
      AddBlackPawnCaptures(c, dst, moves);
    }
    if (col < 7) {
      dst = c;
      dst.DecRow();
      dst.IncCol();
      AddBlackPawnCaptures(c, dst, moves);
    }
  }
  else {
    if (row == 7) {
      return;
    }
    ChessCoord dst = c;
    if (col > 0) {
      dst.IncRow();
      dst.DecCol();
      AddWhitePawnCaptures(c, dst, moves);
    }
    if (col < 7) {
      dst = c;
      dst.IncRow();
      dst.IncCol();
      AddWhitePawnCaptures(c, dst, moves);
    }
  }
}

void ChessBoard::GetCapturesRook(std::vector<Move>& moves, ChessCoord c)
{
  std::vector<ChessCoord> dsts = GetDstRook(c);
  if (checksInfo.blockers.GetBit(c)) {
    ChessCoord pinner = checksInfo.pinners[c.coord];
    for (ChessCoord dst : dsts) {
      if (dst == pinner) {
        moves.push_back(Move(c, dst));
      }
    }
  }
  else {
    for (ChessCoord dst : dsts) {
      if (!GetPiece(dst)->isEmpty()) {
        moves.push_back(Move(c, dst));
      }
    }
  }
}

void ChessBoard::GetCapturesKnight(std::vector<Move>& moves, ChessCoord c)
{
  //knight can't unpin by capturing the pinner.
  if (checksInfo.blockers.GetBit(c)) {
    return;
  }
  std::vector<ChessCoord> dsts = GetDstKnight(c);
  for (ChessCoord dst : dsts) {
    if (!GetPiece(dst)->isEmpty()) {
      moves.push_back(Move(c, dst));
    }
  }
}

void ChessBoard::GetCapturesBishop(std::vector<Move>& moves, ChessCoord c)
{
  std::vector<ChessCoord> dsts = GetDstBishop(c);
  if (checksInfo.blockers.GetBit(c)) {
    ChessCoord pinner = checksInfo.pinners[c.coord];
    for (ChessCoord dst : dsts) {
      if (dst == pinner) {
        moves.push_back(Move(c, dst));
      }
    }
  }
  else {
    for (ChessCoord dst : dsts) {
      if (!GetPiece(dst)->isEmpty()) {
        moves.push_back(Move(c, dst));
      }
    }
  }
}

void ChessBoard::GetCapturesQueen(std::vector<Move> & moves, ChessCoord c)
{
  std::vector<ChessCoord> dsts = GetDstQueen(c);
  if (checksInfo.blockers.GetBit(c)) {
    ChessCoord pinner = checksInfo.pinners[c.coord];
    for (ChessCoord dst : dsts) {
      if (dst == pinner) {
        moves.push_back(Move(c, dst));
      }
    }
  }
  else {
    for (ChessCoord dst : dsts) {
      if (!GetPiece(dst)->isEmpty()) {
        moves.push_back(Move(c, dst));
      }
    }
  }
}

void ChessBoard::GetCapturesKing(std::vector<Move>& moves, ChessCoord c)
{
  uint8_t color = GetPiece(c)->color();
  char r0 = char(c.Row());
  char c0 = char(c.Col());
  const unsigned NUM_DIRS = 8;
  char dir[NUM_DIRS][2] = { {-1,0},{1,0},{0,-1},{0,1},
  {-1,-1},{-1,1},{1,-1},{1,1} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    char col = c0 + dir[d][0];
    char row = r0 + dir[d][1];
    if (col < 0 || col >= BOARD_SIZE) {
      continue;
    }
    if (row < 0 || row >= BOARD_SIZE) {
      continue;
    }
    ChessCoord dst = ChessCoord(col, row);
    if (checksInfo.attacked.GetBit(dst)) {
      continue;
    }
    Piece* dstPiece = GetPiece(dst);
    if (!dstPiece->isEmpty()) {
      if (dstPiece->color() != color) {
        Move m(c, dst);
        moves.push_back(m);
      }
    }
  }
}

void ChessBoard::GetQuiets(std::vector<Move>& moves)
{
  std::vector<ChessCoord>* list = GetPieceList(nextColor);
  for (ChessCoord c : (*list)) {
    if (checksInfo.blockers.GetBit(c)) {
      //if pinned cannot make a quiet move
      continue;
    }
    GetQuiets(moves, c);
  }
}

void ChessBoard::GetQuiets(std::vector<Move>& moves, ChessCoord c)
{
  uint8_t type = GetPiece(c)->type();
  switch (type) {
  case PIECE_PAWN:
    GetQuietsPawn(moves, c);
    break;
  case PIECE_ROOK:
    GetQuietsRook(moves, c);
    break;
  case PIECE_KNIGHT:
    GetQuietsKnight(moves, c);
    break;
  case PIECE_BISHOP:
    GetQuietsBishop(moves, c);
    break;
  case PIECE_QUEEN:
    GetQuietsQueen(moves, c);
    break;
  case PIECE_KING:
    GetQuietsKing(moves, c);
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

void ChessBoard::GetQuietsPawn(std::vector<Move>& moves, ChessCoord c)
{
  uint8_t color = GetPiece(c)->color();
  uint8_t row = c.Row();
  uint8_t col = c.Col();
  if (color == PIECE_BLACK) {
    if (row == 0) {
      //shouldn't happen because it should have been promoted.
      return;
    }
    ChessCoord dst = c;
    dst.DecRow();
    if (GetPiece(dst)->isEmpty()) {
      Move m(c, dst);
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
    ChessCoord dst = c;
    dst.IncRow();
    if (GetPiece(dst)->isEmpty()) {
      Move m(c, dst);
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

void ChessBoard::GetQuietsRook(std::vector<Move>& moves, ChessCoord c)
{
  std::vector<ChessCoord> dsts = GetDstRook(c);
  for (ChessCoord dst : dsts) {
    if (GetPiece(dst)->isEmpty()) {
      moves.push_back(Move(c, dst));
    }
  }
}

void ChessBoard::GetQuietsKnight(std::vector<Move>& moves, ChessCoord c)
{
  std::vector<ChessCoord> dsts = GetDstKnight(c);
  for (ChessCoord dst : dsts) {
    if (GetPiece(dst)->isEmpty()) {
      moves.push_back(Move(c, dst));
    }
  }
}

void ChessBoard::GetQuietsBishop(std::vector<Move>& moves, ChessCoord c) 
{
  std::vector<ChessCoord> dsts = GetDstBishop(c);
  for (ChessCoord dst : dsts) {
    if (GetPiece(dst)->isEmpty()) {
      moves.push_back(Move(c, dst));
    }
  }
}

void ChessBoard::GetQuietsQueen(std::vector<Move>& moves, ChessCoord c)
{
  std::vector<ChessCoord> dsts = GetDstQueen(c);
  for (ChessCoord dst : dsts) {
    if (GetPiece(dst)->isEmpty()) {
      moves.push_back(Move(c, dst));
    }
  }
}

void ChessBoard::GetCastleBlack(std::vector<Move>& moves)
{
  if (castleBK 
    && (!checksInfo.attacked.GetBit(f8)) 
    && (!checksInfo.attacked.GetBit(g8)) 
    && (GetPiece(e8)->type() == PIECE_KING)
    && (GetPiece(h8)->type() == PIECE_ROOK)
    ) {
    if (GetPiece(f8)->isEmpty() && GetPiece(g8)->isEmpty()) {
      Move m(e8, g8);
      moves.push_back(m);
    }
  }
  if (castleBQ
    && (!checksInfo.attacked.GetBit(c8))
    && (!checksInfo.attacked.GetBit(d8))
    && (GetPiece(e8)->type() == PIECE_KING)
    && (GetPiece(a8)->type() == PIECE_ROOK)
    ) {
    if (GetPiece(b8)->isEmpty() && GetPiece(c8)->isEmpty()
      && GetPiece(d8)->isEmpty()) {
      Move m(e8, c8);
      moves.push_back(m);
    }
  }
}

void ChessBoard::GetCastleWhite(std::vector<Move>& moves)
{
  if (castleWK
    && (!checksInfo.attacked.GetBit(f1))
    && (!checksInfo.attacked.GetBit(g1))
    && (GetPiece(e1)->type() == PIECE_KING)
    && (GetPiece(h1)->type() == PIECE_ROOK)
    ) {
    if (GetPiece(f1)->isEmpty() && GetPiece(g1)->isEmpty()) {
      Move m(e1, g1);
      moves.push_back(m);
    }
  }
  if (castleWQ
    && (!checksInfo.attacked.GetBit(c1))
    && (!checksInfo.attacked.GetBit(d1))
    && (GetPiece(e1)->type() == PIECE_KING)
    && (GetPiece(a1)->type() == PIECE_ROOK)
    ) {
    if (GetPiece(b1)->isEmpty() && GetPiece(c1)->isEmpty()
      && GetPiece(d1)->isEmpty()) {
      Move m(e1, c1);
      moves.push_back(m);
    }
  }
}

void ChessBoard::GetQuietsKing(std::vector<Move> & moves, ChessCoord c)
{
  uint8_t color = GetPiece(c)->color();
  char r0 = char(c.Row());
  char c0 = char(c.Col());
  const unsigned NUM_DIRS = 8;
  char dir[NUM_DIRS][2] = { {-1,0},{1,0},{0,-1},{0,1},
  {-1,-1},{-1,1},{1,-1},{1,1} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    char col = c0 + dir[d][0];
    char row = r0 + dir[d][1];
    if (col < 0 || col >= BOARD_SIZE) {
      continue;
    }
    if (row < 0 || row >= BOARD_SIZE) {
      continue;
    }
    ChessCoord dst = ChessCoord(col, row);
    if (checksInfo.attacked.GetBit(dst)) {
      continue;
    }
    Piece* dstPiece = GetPiece(col, row);
    if (dstPiece->isEmpty()) {
      Move m(c, dst);
      moves.push_back(m);
    }
  }

  if (color == uint8_t(PIECE_BLACK)) {
    GetCastleBlack(moves);
  }
  else {
    GetCastleWhite(moves);
  }
}

int ChessBoard::ApplyMove(const Move& m)
{
  Piece* p = GetPiece(m.src);
  uint8_t color = p->color();
  if (m.src == m.dst) {
    return -1;
  }
  if (p->isEmpty()) {
    return -2;
  }
  if (color != nextColor) {
    return -3;
  }
  
  hashHistory.push_back(HashVal());
  hash.Update(*this, m);
  
  nextColor = !color;
  uint8_t type = p->type();
  hasEnPassant = false;
  enPassantDst = ChessCoord();
  halfMoves++;
  CastleMove castling = CASTLE_NONE;
  //is this move en Passant
  bool isEnPassant=false;
  if (color == PIECE_BLACK ){
    switch (type) {
    case PIECE_PAWN:
      if (m.src.Row() == 6 && m.dst.Row() == 4) {
        enPassantDst = ChessCoord(m.dst.Col(), 5);
        activateEnPassantBlack();
      }
      if (m.src.Col() != m.dst.Col() && GetPiece(m.dst)->isEmpty()) {
        isEnPassant = true;
      }
      halfMoves = 0;
      break;
    case PIECE_KING:
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
    case PIECE_ROOK:
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
    case PIECE_PAWN:
      if (m.src.Row() == 1 && m.dst.Row() == 3) {
        enPassantDst = ChessCoord(m.dst.Col(), 2);
        activateEnPassantWhite();
      }
      if (m.src.Col() != m.dst.Col() && GetPiece(m.dst)->isEmpty()) {
        isEnPassant = true;
      }
      halfMoves = 0;
      break;
    case PIECE_KING:
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
    case PIECE_ROOK:
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
    *dstp = m.promo;
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
    MovePiece(a8, d8);
    hasCastled[0] = true;
    break;
  case CASTLE_BK:
    MovePiece(h8, f8);
    hasCastled[0] = true;
    break;
  case CASTLE_WQ:
    MovePiece(a1, d1);
    hasCastled[1] = true;
    break;
  case CASTLE_WK:
    MovePiece(h1, f1);
    hasCastled[1] = true;
    break;
  }

  moveCount++;
  return 0;
}

UndoMove ChessBoard::GetUndoMove(const Move& m)
{
  UndoMove u;
  u.m = m;
  u.captured = *(GetPiece(m.dst));
  Piece srcPiece = (*GetPiece(m.src));
  if (hasEnPassant && srcPiece.type() == PIECE_PAWN && m.dst == enPassantDst){
    u.isEnPassant = true;
  }
  u.halfMoves = halfMoves;
  u.hasEnPassant = hasEnPassant;
  u.enPassantDst = enPassantDst;

  u.isPromo = (m.promo != PIECE_EMPTY);
  if (nextColor == PIECE_BLACK) {
    u.castleK = castleBK;
    u.castleQ = castleBQ;
  }
  else {
    u.castleK = castleWK;
    u.castleQ = castleWQ;
  }
  return u;
}

void ChessBoard::Undo(const UndoMove& u)
{
  bool isCastle = false;
  //src piece is now at dst.
  Piece srcPiece = *GetPiece(u.m.dst);
  if (srcPiece.type() == PIECE_KING) {
    if (u.m.src == e1) {
      if (u.m.dst == g1) {
        isCastle = true;
        MovePiece(f1, h1);
      }
      else if (u.m.dst == c1) {
        isCastle = true;
        MovePiece(d1, a1);
      }
    }
    else if (u.m.src == e8) {
      if (u.m.dst == g8) {
        isCastle = true;
        MovePiece(f8, h8);
      }
      else if (u.m.dst == c8) {
        isCastle = true;
        MovePiece(d8, a8);
      }
    }
  }
  
  FlipTurn();
  MovePiece(u.m.dst, u.m.src);
  if (u.isPromo) {
    Piece* srcPiece = GetPiece(u.m.src);
    srcPiece->SetType(PIECE_PAWN);
  }
  if (isCastle) {
    //castling either direction is available again
    if (nextColor == PIECE_BLACK) {
      castleBQ = true;
      castleBK = true;
      hasCastled[PIECE_BLACK] = false;
    }
    else {
      castleWQ = true;
      castleWK = true;
      hasCastled[PIECE_WHITE] = false;
    }
  }
  else if (u.isEnPassant) {
    hasEnPassant = true;
    enPassantDst = u.m.dst;

    ChessCoord captured = enPassantDst;
    if (nextColor == PIECE_BLACK) {
      captured.IncRow();
      AddPiece(captured, WHITE_PAWN);
    }
    else {
      captured.DecRow();
      AddPiece(captured, BLACK_PAWN);
    }
  }
  else if (u.captured != PIECE_EMPTY) {
    AddPiece(u.m.dst, u.captured);
  }
  
  halfMoves = u.halfMoves;

  if (nextColor == PIECE_BLACK) {
    castleBK = u.castleK;
    castleBQ = u.castleQ;
    fullMoves--;
  }
  else {
    castleWK = u.castleK;
    castleWQ = u.castleQ;
  }
  moveCount--;
  hasEnPassant = u.hasEnPassant;
  enPassantDst = u.enPassantDst;
  
  if (hashHistory.size() > 0) {
    uint64_t oldHash = hashHistory.back();
    hash.hash = oldHash;
    hashHistory.pop_back();
  }
  else {
    hash.Set(*this);
    std::cout << "warning: no more steps to undo.\n";
  }
}

char PieceFEN(const Piece& info)
{
  uint8_t type = info.type();
  char c;
  const char LOWER_DIFF = 'a' - 'A';
  char PieceChar[PIECE_NUM_TYPES] = {
    '0','P','R','N','B','Q','K'
  };
  c = PieceChar[type];
  if (info.color() == PIECE_BLACK) {
    c += LOWER_DIFF;
  }
  return c;
}

Piece Char2Piece(char c)
{
  Piece info(0);
  const char LOWER_DIFF = 'a' - 'A';
  if (c > 'a') {
    info.SetColor(PIECE_BLACK);
    c -= LOWER_DIFF;
  }
  else {
    info.SetColor(PIECE_WHITE);
  }
  switch (c) {
  case 'P':
    info.SetType(PIECE_PAWN);
    break;
  case 'R':
    info.SetType(PIECE_ROOK);
    break;
  case 'N':
    info.SetType(PIECE_KNIGHT);
    break;
  case 'B':
    info.SetType(PIECE_BISHOP);
    break;
  case 'Q':
    info.SetType(PIECE_QUEEN);
    break;
  case 'K':
    info.SetType(PIECE_KING);
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

void ChessBoard::InitHash()
{
  hashHistory.clear();
  hash.Set(*this);
}

uint64_t ChessBoard::HashVal()const
{
  return hash.hash;
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
        Piece p = Char2Piece(c);
        AddPiece(ChessCoord(col, row), p);
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
    nextColor = PIECE_WHITE;
  }
  else {
    nextColor = PIECE_BLACK;
  }
  strIdx += 2;

  c = fen[strIdx];
  castleWK = false;
  castleWQ = false;
  castleBK = false;
  castleBQ = false;
  while (c != ' ' && strIdx<fen.size()) {
    if (c == '-') {
      strIdx++;
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
    if (nextColor == PIECE_WHITE) {
      activateEnPassantWhite();
    }
    else {
      activateEnPassantBlack();
    }
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
      if (p.isEmpty()) {
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
  if (nextColor == PIECE_WHITE) {
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

bool ChessBoard::AddPiece(ChessCoord c, Piece info)
{
  Piece* p = GetPiece(c);
  if (!p->isEmpty()) {
    return false;
  }
  *p = info;

  std::vector<ChessCoord>* list = GetPieceList(p->color());
  list->push_back(c);
  return true;
}

bool ChessBoard::RemovePiece(ChessCoord c)
{
  Piece* p = GetPiece(c);
  if (p->isEmpty()) {
    return false;
  }
  std::vector<ChessCoord>* list = GetPieceList(p->color());
  for (auto it = list->begin(); it != list->end(); it++) {
    if ((*it) == c){
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

  std::vector<ChessCoord>* list = GetPieceList(p->color());
  for (size_t i = 0; i < list->size(); i++) {
    if ( (*list)[i] == src) {
      Piece* dstPiece = GetPiece(dst);
      (*list)[i] = dst;
      dstPiece->info = p->info;
      p->clear();
      break;
    }
  }
  return true;
}

void ChessBoard::Clear()
{
  pieces[0].clear();
  pieces[1].clear();
  for (size_t i = 0; i < board.size(); i++) {
    board[i].clear();
  }

  hasEnPassant = false;
  fullMoves = 0;
  halfMoves = 0;
  moveCount = 0;
  castleBK = true;
  castleBQ = true;
  castleWK = true;
  castleWQ = true;

  hasCastled[0] = false;
  hasCastled[1] = false;
}

void ComputeChecksPawn(ChecksInfo & info, ChessCoord coord, uint8_t color,
  ChessCoord kingCoord)
{
  uint8_t offsetl=7,offsetr=9;
  if (color == PIECE_BLACK) {
    if (coord.Col() > 0) {
      ChessCoord dst = coord;
      dst.coord -= offsetl;
      info.attacked.SetBit(dst.coord);
      if (dst == kingCoord) {
        info.attackers.push_back(coord);
      }
    }
    if (coord.Col() < 7) {
      ChessCoord dst = coord;
      dst.coord -= offsetr;
      info.attacked.SetBit(dst.coord);
      if (dst == kingCoord) {
        info.attackers.push_back(coord);
      }
    }
  }
  else {
    if(coord.Col()>0){
      ChessCoord dst = coord;
      dst.coord += offsetl;
      info.attacked.SetBit(dst.coord);
      if (dst == kingCoord) {
        info.attackers.push_back(coord);
      }
    }
    if (coord.Col() < 7) {
      ChessCoord dst = coord;
      dst.coord += offsetr;
      info.attacked.SetBit(dst.coord);
      if (dst == kingCoord) {
        info.attackers.push_back(coord);
      }
    }
  }
}

void ComputeChecksKnight(ChecksInfo& info, ChessCoord coord, uint8_t color,
  ChessCoord kingCoord)
{
  char r0 = char(coord.Row());
  char c0 = char(coord.Col());
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
    ChessCoord dst(col, row);
    info.attacked.SetBit(dst.coord);
    if (dst == kingCoord) {
      info.attackers.push_back(coord);
    }
  }
}

void ComputeChecksKing(ChecksInfo& info, ChessCoord coord, uint8_t color,
  ChessCoord kingCoord)
{
  char r0 = char(coord.Row());
  char c0 = char(coord.Col());
  const unsigned NUM_DIRS = 8;
  char dir[NUM_DIRS][2] = { {-1,-1},{-1,0},{-1,1},{0,-1},
    {0,1},{1,-1},{1,0},{1,1} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    char col = c0 + dir[d][0];
    char row = r0 + dir[d][1];
    if (col < 0 || col >= BOARD_SIZE) {
      continue;
    }
    if (row < 0 || row >= BOARD_SIZE) {
      continue;
    }
    ChessCoord dst(col, row);
    info.attacked.SetBit(dst.coord);
    if (dst == kingCoord) {
      info.attackers.push_back(coord);
    }
  }
}

void InsertUnique(std::vector<ChessCoord>& coords, ChessCoord c)
{
  auto it = std::find(coords.begin(), coords.end(), c);
  if (it != coords.end()) {
    coords.push_back(c);
  }
}

void ChessBoard::ComputeChecksRay(ChecksInfo& checks, ChessCoord coord, uint8_t color,
  ChessCoord kingCoord, char dx, char dy)
{
  char row = char(coord.Row());
  char col = char(coord.Col());
  //first opposite color pieces along the line.
  ChessCoord firstHit(255);
  bool attackKing = false;
  for (char step = 0; step < BOARD_SIZE; step++) {
    col += dx;
    if (col < 0 || col >= BOARD_SIZE) {
      break;
    }
    row += dy;
    if (row < 0 || row >= BOARD_SIZE) {
      break;
    }
    ChessCoord dst(col, row);
    if (dst == kingCoord) {
      if (!firstHit.inBound()) {
        checks.attackers.push_back(coord);
        attackKing = true;
        checks.attacked.SetBit(dst.coord);
        continue;
      }
      else {
        checks.blockers.SetBit(firstHit.coord);
        checks.pinners[firstHit.coord] = coord;
        break;
      }
    }
    Piece* dstPiece = GetPiece(dst);
    if (dstPiece->isEmpty()) {
      if (!firstHit.inBound()) {
        checks.attacked.SetBit(dst.coord);
      }
      continue;
    }

    if (dstPiece->color() == uint8_t(color)) {
      if (!firstHit.inBound()) {
        //this piece is guarded by attacker, so king can't
        //capture this piece.
        checks.attacked.SetBit(dst.coord);
      }
      break;
    }
    else {
      //already attacking king and then hit another piece,
      //no need to continue
      if (attackKing) {
        break;
      }
      //has a hit before
      if (firstHit.inBound()) {
        break;
      }
      else {
        firstHit = dst;
      }
    }
  }
}

void ChessBoard::ComputeChecksRook(ChecksInfo& checks, ChessCoord coord, uint8_t color,
  ChessCoord kingCoord)
{
  const unsigned NUM_DIRS = 4;
  char dir[NUM_DIRS][2] = { {-1,0},{1,0},{0,-1},{0,1} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    ComputeChecksRay(checks, coord, color, kingCoord, dir[d][0], dir[d][1]);
  }
}

void ChessBoard::ComputeChecksBishop(ChecksInfo& checks, ChessCoord coord, uint8_t color,
  ChessCoord kingCoord)
{
  const unsigned NUM_DIRS = 4;
  char dir[NUM_DIRS][2] = { {-1,-1},{-1,1},{1,-1},{1,1} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    ComputeChecksRay(checks, coord, color, kingCoord, dir[d][0], dir[d][1]);
  }
}

void ChessBoard::ComputeChecksQueen(ChecksInfo& checks, ChessCoord coord, uint8_t color,
  ChessCoord kingCoord)
{
  const unsigned NUM_DIRS = 8;
  char dir[NUM_DIRS][2] = { {-1,0},{1,0},{0,-1},{0,1} ,
    {-1,-1},{-1,1},{1,-1},{1,1} };
  for (unsigned d = 0; d < NUM_DIRS; d++) {
    ComputeChecksRay(checks, coord, color, kingCoord, dir[d][0], dir[d][1]);
  }
}

ChecksInfo ChessBoard::ComputeChecks()
{
  ChecksInfo checks;
  std::vector<ChessCoord>* list;
  std::vector<ChessCoord>* attackerList;
  uint8_t attackerColor;
  if (nextColor == PIECE_BLACK) {
    list = &(pieces[0]);
    attackerList = &(pieces[1]);
    attackerColor = PIECE_WHITE;
  }
  else {
    list = &(pieces[1]);
    attackerList = &(pieces[0]);
    attackerColor = PIECE_BLACK;
  }
  ChessCoord kingCoord;
  for (ChessCoord c : (*list)) {
    if (GetPiece(c)->type() == PIECE_KING) {
      kingCoord = c;
      break;
    }
  }
  checks.kingCoord = kingCoord;
  for (ChessCoord c : (*attackerList)) {
    Piece* p = GetPiece(c);
    uint8_t type = p->type();
    switch (type) {
    case PIECE_PAWN:
      ComputeChecksPawn(checks, c, attackerColor, kingCoord);
      break;
    case PIECE_KNIGHT:
      ComputeChecksKnight(checks, c, attackerColor, kingCoord);
      break;
    case PIECE_KING:
      ComputeChecksKing(checks, c, attackerColor, kingCoord);
      break;
    case PIECE_ROOK:
      ComputeChecksRook(checks, c, attackerColor, kingCoord);
      break;
    case PIECE_BISHOP:
      ComputeChecksBishop(checks, c, attackerColor, kingCoord);
      break;
    case PIECE_QUEEN:
      ComputeChecksQueen(checks, c, attackerColor, kingCoord);
      break;
    }
  }

  return checks;
}

void ChessBoard::activateEnPassantBlack() {
  if (enPassantDst.coord != a6) {
    Piece* oppoPiece = GetPiece(enPassantDst.coord - 9);
    if (oppoPiece->info==WHITE_PAWN) {
      hasEnPassant = true;
      return;
    }
  }

  if (enPassantDst.coord != h6) {
    Piece* oppoPiece = GetPiece(enPassantDst.coord - 7);
    if (oppoPiece->info == WHITE_PAWN) {
      hasEnPassant = true;
      return;
    }
  }
}

void ChessBoard::activateEnPassantWhite() {
  if (enPassantDst.coord != a3) {
    Piece* oppoPiece = GetPiece(enPassantDst.coord + 7);
    if (oppoPiece->info == BLACK_PAWN) {
      hasEnPassant = true;
      return;
    }
  }

  if (enPassantDst.coord != h3 ) {
    Piece* oppoPiece = GetPiece(enPassantDst.coord + 9);
    if (oppoPiece->info == BLACK_PAWN) {
      hasEnPassant = true;
      return;
    }
  }
}
