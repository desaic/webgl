#include "BoardEval.h"

#include <algorithm>
#include <cstdlib>

const int BoardEval::MAX_SCORE = 100000;
const int BoardEval::CASTLE_SCORE = 0;  // unused: king safety now comes from the king tables

namespace {

// Tapered evaluation: every term has a middlegame and an endgame value, blended by how much
// non-pawn material is left. Piece-square tables are the well known PeSTO set, written from White's
// side with a8 first, so White indexes them with coord ^ 56 and Black with coord.
const int mgValue[7] = {0, 82, 477, 337, 365, 1025, 0};
const int egValue[7] = {0, 94, 512, 281, 297, 936, 0};
const int phaseWeight[7] = {0, 0, 2, 1, 1, 4, 0};

const int mgPawn[64] = {0,   0,   0,   0,   0,   0,  0,   0,   98,  134, 61,  95,  68, 126, 34,  -11,
                        -6,  7,   26,  31,  65,  56, 25,  -20, -14, 13,  6,   21,  23, 12,  17,  -23,
                        -27, -2,  -5,  12,  17,  6,  10,  -25, -26, -4,  -4,  -10, 3,  3,   33,  -12,
                        -35, -1,  -20, -23, -15, 24, 38,  -22, 0,   0,   0,   0,   0,  0,   0,   0};
const int egPawn[64] = {0,   0,   0,   0,   0,   0,   0,   0,   178, 173, 158, 134, 147, 132, 165, 187,
                        94,  100, 85,  67,  56,  53,  82,  84,  32,  24,  13,  5,   -2,  4,   17,  17,
                        13,  9,   -3,  -7,  -7,  -8,  3,   -1,  4,   7,   -6,  1,   0,   -5,  -1,  -8,
                        13,  8,   8,   10,  13,  0,   2,   -7,  0,   0,   0,   0,   0,   0,   0,   0};
const int mgKnight[64] = {-167, -89, -34, -49, 61,  -97, -15, -107, -73, -41, 72,  36,  23,  62,  7,   -17,
                          -47,  60,  37,  65,  84,  129, 73,  44,   -9,  17,  19,  53,  37,  69,  18,  22,
                          -13,  4,   16,  13,  28,  19,  21,  -8,   -23, -9,  12,  10,  19,  17,  25,  -16,
                          -29,  -53, -12, -3,  -1,  18,  -14, -19,  -105, -21, -58, -33, -17, -28, -19, -23};
const int egKnight[64] = {-58, -38, -13, -28, -31, -27, -63, -99, -25, -8,  -25, -2,  -9,  -25, -24, -52,
                          -24, -20, 10,  9,   -1,  -9,  -19, -41, -17, 3,   22,  22,  22,  11,  8,   -18,
                          -18, -6,  16,  25,  16,  17,  4,   -18, -23, -3,  -1,  15,  10,  -3,  -20, -22,
                          -42, -20, -10, -5,  -2,  -20, -23, -44, -29, -51, -23, -15, -22, -18, -50, -64};
const int mgBishop[64] = {-29, 4,  -82, -37, -25, -42, 7,   -8,  -26, 16,  -18, -13, 30,  59,  18,  -47,
                          -16, 37, 43,  40,  35,  50,  37,  -2,  -4,  5,   19,  50,  37,  37,  7,   -2,
                          -6,  13, 13,  26,  34,  12,  10,  4,   0,   15,  15,  15,  14,  27,  18,  10,
                          4,   15, 16,  0,   7,   21,  33,  1,   -33, -3,  -14, -21, -13, -12, -39, -21};
const int egBishop[64] = {-14, -21, -11, -8,  -7, -9,  -17, -24, -8,  -4, 7,  -12, -3,  -13, -4, -14,
                          2,   -8,  0,   -1,  -2, 6,   0,   4,   -3,  9,  12, 9,   14,  10,  3,  2,
                          -6,  3,   13,  19,  7,  10,  -3,  -9,  -12, -3, 8,  10,  13,  3,   -7, -15,
                          -14, -18, -7,  -1,  4,  -9,  -15, -27, -23, -9, -23, -5,  -9,  -16, -5, -17};
const int mgRook[64] = {32,  42,  32,  51, 63, 9,  31,  43,  27,  32,  58,  62, 80, 67, 26,  44,
                        -5,  19,  26,  36, 17, 45, 61,  16,  -24, -11, 7,   26, 24, 35, -8,  -20,
                        -36, -26, -12, -1, 9,  -7, 6,   -23, -45, -25, -16, -17, 3, 0,  -5,  -33,
                        -44, -16, -20, -9, -1, 11, -6,  -71, -19, -13, 1,   17, 16, 7,  -37, -26};
const int egRook[64] = {13, 10, 18, 15, 12, 12,  8,   5,   11, 13, 13, 11, -3, 3,   8,   3,
                        7,  7,  7,  5,  4,  -3,  -5,  -3,  4,  3,  13, 1,  2,  1,   -1,  2,
                        3,  5,  8,  4,  -5, -6,  -8,  -11, -4, 0,  -5, -1, -7, -12, -8,  -16,
                        -6, -6, 0,  2,  -9, -9,  -11, -3,  -9, 2,  3,  -1, -5, -13, 4,   -20};
const int mgQueen[64] = {-28, 0,   29,  12,  59,  44,  43,  45,  -24, -39, -5,  1,   -16, 57,  28,  54,
                         -13, -17, 7,   8,   29,  56,  47,  57,  -27, -27, -16, -16, -1,  17,  -2,  1,
                         -9,  -26, -9,  -10, -2,  -4,  3,   -3,  -14, 2,   -11, -2,  -5,  2,   14,  5,
                         -35, -8,  11,  2,   8,   15,  -3,  1,   -1,  -18, -9,  10,  -15, -25, -31, -50};
const int egQueen[64] = {-9,  22,  22,  27,  27,  19,  10,  20,  -17, 20,  32,  41,  58,  25,  30,  0,
                         -20, 6,   9,   49,  47,  35,  19,  9,   3,   22,  24,  45,  57,  40,  57,  36,
                         -18, 28,  19,  47,  31,  34,  39,  23,  -16, -27, 15,  6,   9,   17,  10,  5,
                         -22, -23, -30, -16, -16, -23, -36, -32, -33, -28, -22, -43, -5,  -32, -20, -41};
const int mgKing[64] = {-65, 23,  16,  -15, -56, -34, 2,   13,  29,  -1,  -20, -7,  -8,  -4,  -38, -29,
                        -9,  24,  2,   -16, -20, 6,   22,  -22, -17, -20, -12, -27, -30, -25, -14, -36,
                        -49, -1,  -27, -39, -46, -44, -33, -51, -14, -14, -22, -46, -44, -30, -15, -27,
                        1,   7,   -8,  -64, -43, -16, 9,   8,   -15, 36,  12,  -54, 8,   -28, 24,  14};
const int egKing[64] = {-74, -35, -18, -18, -11, 15,  4,   -17, -12, 17,  14,  17,  17,  38,  23,  11,
                        10,  17,  23,  15,  20,  45,  44,  13,  -8,  22,  24,  27,  26,  33,  26,  3,
                        -18, -4,  21,  24,  27,  23,  9,   -11, -19, -3,  11,  21,  23,  16,  7,   -9,
                        -27, -11, 4,   13,  14,  4,   -5,  -17, -53, -34, -21, -11, -28, -14, -24, -43};

const int* const mgTable[7] = {nullptr, mgPawn, mgRook, mgKnight, mgBishop, mgQueen, mgKing};
const int* const egTable[7] = {nullptr, egPawn, egRook, egKnight, egBishop, egQueen, egKing};

// passed pawn bonus by rank counted from the pawn's own side (index 0..7)
const int passedMg[8] = {0, 0, 5, 10, 20, 40, 70, 0};
const int passedEg[8] = {0, 5, 10, 25, 45, 80, 130, 0};

int Dist(ChessCoord a, ChessCoord c) {
  return std::abs(int(a.Row()) - int(c.Row())) + std::abs(int(a.Col()) - int(c.Col()));
}

int CenterDist(ChessCoord c) {
  int r = c.Row(), f = c.Col();
  return std::max(3 - std::min(r, 7 - r), 0) + std::max(3 - std::min(f, 7 - f), 0);
}

struct Side {
  int mg = 0, eg = 0;
  int count[7] = {0};
  int nonPawn = 0;  // material without pawns, centipawns
  int bishopSq[2] = {0, 0};
  int pawnsOnFile[8] = {0};
  ChessCoord king;
};

}  // namespace

// true if neither side can force mate: bare kings, one minor piece, or two knights vs bare king,
// or one minor each with no pawns (a rare lucky mate is still possible but the eval is 0)
bool BoardEval::InsufficientMaterial(const ChessBoard& b) {
  int minors[2] = {0, 0}, knights[2] = {0, 0};
  for (int c = 0; c < 2; c++) {
    for (ChessCoord sq : b.pieces[c]) {
      uint8_t t = b.GetPiece(sq)->type();
      if (t == PIECE_PAWN || t == PIECE_ROOK || t == PIECE_QUEEN) return false;
      if (t == PIECE_KNIGHT) { minors[c]++; knights[c]++; }
      if (t == PIECE_BISHOP) minors[c]++;
    }
  }
  int total = minors[0] + minors[1];
  if (total <= 1) return true;
  // K+NN v K cannot be forced; any other mix can in principle
  if (total == 2 && (knights[0] == 2 || knights[1] == 2) && (minors[0] == 0 || minors[1] == 0)) return true;
  return false;
}

int BoardEval::Eval(const ChessBoard& b) {
  if (InsufficientMaterial(b)) return 0;

  Side s[2];  // 0 black, 1 white
  int phase = 0;
  for (int c = 0; c < 2; c++) {
    Side& me = s[c];
    for (ChessCoord sq : b.pieces[c]) {
      int t = b.GetPiece(sq)->type();
      int idx = c == PIECE_WHITE ? (sq.coord ^ 56) : sq.coord;
      me.count[t]++;
      me.mg += mgValue[t] + (mgTable[t] ? mgTable[t][idx] : 0);
      me.eg += egValue[t] + (egTable[t] ? egTable[t][idx] : 0);
      phase += phaseWeight[t];
      if (t == PIECE_KING) me.king = sq;
      else if (t == PIECE_PAWN) me.pawnsOnFile[sq.Col()]++;
      else me.nonPawn += mgValue[t];
    }
  }

  for (int c = 0; c < 2; c++) {
    Side& me = s[c];
    const Side& opp = s[1 - c];
    if (me.count[PIECE_BISHOP] >= 2) { me.mg += 30; me.eg += 45; }

    for (ChessCoord sq : b.pieces[c]) {
      int t = b.GetPiece(sq)->type();
      int f = sq.Col();
      int rank = c == PIECE_WHITE ? sq.Row() : 7 - sq.Row();  // 0 = own back rank
      if (t == PIECE_PAWN) {
        // doubled: another own pawn ahead on the same file is counted from the rear pawn only
        bool isolated = (f == 0 || !me.pawnsOnFile[f - 1]) && (f == 7 || !me.pawnsOnFile[f + 1]);
        if (isolated) { me.mg -= 12; me.eg -= 16; }
        // passed: no enemy pawn ahead on this or adjacent files
        bool passed = true;
        bool doubled = false;
        for (ChessCoord o : b.pieces[c]) {
          if (o.coord == sq.coord || b.GetPiece(o)->type() != PIECE_PAWN || o.Col() != f) continue;
          int orank = c == PIECE_WHITE ? o.Row() : 7 - o.Row();
          if (orank > rank) doubled = true;
        }
        if (doubled) { me.mg -= 10; me.eg -= 20; }
        for (ChessCoord o : b.pieces[1 - c]) {
          if (b.GetPiece(o)->type() != PIECE_PAWN || std::abs(int(o.Col()) - f) > 1) continue;
          int orank = c == PIECE_WHITE ? o.Row() : 7 - o.Row();
          if (orank > rank) { passed = false; break; }
        }
        if (passed && !doubled) { me.mg += passedMg[rank]; me.eg += passedEg[rank]; }
      } else if (t == PIECE_ROOK) {
        if (!me.pawnsOnFile[f]) {
          bool open = !opp.pawnsOnFile[f];
          me.mg += open ? 22 : 10;
          me.eg += open ? 12 : 6;
        }
      }
    }

    // pawn shield: friendly pawns on the three files in front of a king on the back two ranks
    int kr = c == PIECE_WHITE ? me.king.Row() : 7 - me.king.Row();
    if (kr <= 1) {
      int shield = 0;
      for (int f = std::max(0, int(me.king.Col()) - 1); f <= std::min(7, int(me.king.Col()) + 1); f++) {
        for (ChessCoord o : b.pieces[c]) {
          if (b.GetPiece(o)->type() != PIECE_PAWN || o.Col() != f) continue;
          int orank = c == PIECE_WHITE ? o.Row() : 7 - o.Row();
          if (orank > kr && orank <= kr + 2) { shield++; break; }
        }
      }
      me.mg += 9 * shield;
    }
  }

  int mg = s[1].mg - s[0].mg;
  int eg = s[1].eg - s[0].eg;

  // mop-up: with a decisive material lead and bare-ish opponent, drive the losing king to the edge
  // and bring the winning king close so K+Q v K and K+R v K can be converted
  int lead = (s[1].nonPawn + 100 * s[1].count[PIECE_PAWN]) - (s[0].nonPawn + 100 * s[0].count[PIECE_PAWN]);
  if (std::abs(lead) >= 300) {
    int w = lead > 0 ? PIECE_WHITE : PIECE_BLACK;
    int l = 1 - w;
    int bonus = 10 * CenterDist(s[l].king) + 4 * (14 - Dist(s[w].king, s[l].king));
    // only meaningful once the loser has little left to defend with
    if (s[l].nonPawn <= 900) eg += (w == PIECE_WHITE ? bonus : -bonus);
  }

  if (phase > 24) phase = 24;
  int score = (mg * phase + eg * (24 - phase)) / 24;
  return b.nextColor == PIECE_WHITE ? score : -score;
}
