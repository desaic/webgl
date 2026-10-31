#include "Search.h"

#include "BoardEval.h"

#include <algorithm>

namespace {

const int kPieceValue[PIECE_NUM_TYPES] = {0, 100, 500, 300, 300, 900, 20000};

uint16_t Pack(const Move& m) {
  return uint16_t(m.src.coord | (m.dst.coord << 6) | ((m.promo.info & 15) << 12));
}

bool SameMove(const Move& m, uint16_t packed) { return Pack(m) == packed; }

bool IsCapture(const ChessBoard& b, const Move& m) {
  if (!b.board[m.dst.coord].isEmpty()) return true;
  // en passant: a pawn changing file onto an empty square
  return b.board[m.src.coord].type() == PIECE_PAWN && m.src.Col() != m.dst.Col();
}

}  // namespace

Search::Search() : stopped_(false), timed_(false), nodes_(0) {
  size_t bits = 20;
  table_.resize(size_t(1) << bits);
  tableMask_ = (uint64_t(1) << bits) - 1;
  for (auto& a : history_) for (auto& r : a) for (int& v : r) v = 0;
}

void Search::ClearTable() { std::fill(table_.begin(), table_.end(), Entry()); }

bool Search::OutOfTime() {
  if (stopped_) return true;
  // reading the clock is comparatively slow, so only do it every 1024 nodes
  if (timed_ && (nodes_ & 1023) == 0 && std::chrono::steady_clock::now() >= deadline_) {
    stopped_ = true;
  }
  return stopped_;
}

bool Search::IsRepetition(const ChessBoard& b) const {
  // positions since the last irreversible move, same side to move only (every 2nd entry)
  const std::vector<uint64_t>& h = b.hashHistory;
  uint64_t cur = b.HashVal();
  int limit = std::min<int>(b.halfMoves, int(h.size()));
  for (int i = 2; i <= limit; i += 2) {
    if (h[h.size() - i] == cur) return true;
  }
  return false;
}

void Search::OrderMoves(const ChessBoard& b, std::vector<Move>& moves, uint16_t ttMove,
                        int ply) const {
  std::vector<int> score(moves.size());
  for (size_t i = 0; i < moves.size(); i++) {
    const Move& m = moves[i];
    int s = 0;
    if (ttMove && SameMove(m, ttMove)) {
      s = 1000000;
    } else if (IsCapture(b, m)) {
      int victim = b.board[m.dst.coord].isEmpty() ? 100 : kPieceValue[b.board[m.dst.coord].type()];
      s = 100000 + victim * 10 - kPieceValue[b.board[m.src.coord].type()] / 10;
    } else if (!m.promo.isEmpty()) {
      s = 90000;
    } else if (ply < MaxPly && (killers_[ply][0].src == m.src && killers_[ply][0].dst == m.dst)) {
      s = 80000;
    } else if (ply < MaxPly && (killers_[ply][1].src == m.src && killers_[ply][1].dst == m.dst)) {
      s = 70000;
    } else {
      s = std::min(history_[b.nextColor][m.src.coord][m.dst.coord], 60000);
    }
    score[i] = s;
  }
  std::vector<size_t> idx(moves.size());
  for (size_t i = 0; i < idx.size(); i++) idx[i] = i;
  std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t c) { return score[a] > score[c]; });
  std::vector<Move> sorted(moves.size());
  for (size_t i = 0; i < idx.size(); i++) sorted[i] = moves[idx[i]];
  moves.swap(sorted);
}

namespace {
// mate scores are relative to the root; in the table they are stored relative to the node
int ToTable(int s, int ply) {
  if (s > Search::MateScore - Search::MaxPly * 2) return s + ply;
  if (s < -Search::MateScore + Search::MaxPly * 2) return s - ply;
  return s;
}
int FromTable(int s, int ply) {
  if (s > Search::MateScore - Search::MaxPly * 2) return s - ply;
  if (s < -Search::MateScore + Search::MaxPly * 2) return s + ply;
  return s;
}
}  // namespace

int Search::Quiesce(ChessBoard& b, int ply, int alpha, int beta) {
  nodes_++;
  if (OutOfTime()) return 0;
  std::vector<Move> moves = b.GetMoves();
  bool inCheck = b.IsInCheck();
  if (moves.empty()) return inCheck ? -(MateScore - ply) : 0;
  if (ply >= MaxPly) return BoardEval::Eval(b);

  int stand = -2 * MateScore;
  if (!inCheck) {
    stand = BoardEval::Eval(b);
    if (stand >= beta) return stand;
    alpha = std::max(alpha, stand);
    // only noisy moves; being in check means every evasion has to be considered instead
    moves.erase(std::remove_if(moves.begin(), moves.end(),
                               [&](const Move& m) { return !IsCapture(b, m) && m.promo.isEmpty(); }),
                moves.end());
  }
  OrderMoves(b, moves, 0, ply);
  for (const Move& m : moves) {
    if (!inCheck && m.promo.isEmpty()) {
      // delta pruning: even winning the victim plus a margin cannot reach alpha
      int victim = b.board[m.dst.coord].isEmpty() ? 100 : kPieceValue[b.board[m.dst.coord].type()];
      if (stand + victim + 200 < alpha) continue;
    }
    UndoMove u = b.GetUndoMove(m);
    b.ApplyMove(m);
    int score = -Quiesce(b, ply + 1, -beta, -alpha);
    b.Undo(u);
    if (stopped_) return 0;
    if (score >= beta) return score;
    alpha = std::max(alpha, score);
  }
  return alpha;
}

int Search::Negamax(ChessBoard& b, int depth, int ply, int alpha, int beta, bool allowNull) {
  if (b.halfMoves >= 100 || IsRepetition(b)) return 0;
  if (depth <= 0) return Quiesce(b, ply, alpha, beta);
  nodes_++;
  if (OutOfTime()) return 0;

  uint64_t h = b.HashVal();
  Entry& e = table_[h & tableMask_];
  uint16_t ttMove = 0;
  if (e.hash == h && e.depth >= 0) {
    ttMove = e.move;
    if (e.depth >= depth) {
      int s = FromTable(e.score, ply);
      if (e.bound == 0) return s;
      if (e.bound == 2 && s >= beta) return s;
      if (e.bound == 1 && s <= alpha) return s;
    }
  }

  std::vector<Move> moves = b.GetMoves();
  bool inCheck = b.IsInCheck();
  if (moves.empty()) return inCheck ? -(MateScore - ply) : 0;
  if (inCheck && ply < MaxPly) depth++;

  // Null move pruning: if passing the move still leaves a score >= beta, the real moves will too.
  // Unsafe in zugzwang, where passing would be better than any move, which happens in pawn
  // endings and with very little material. So it is off when in check, when the side to move has
  // under about a rook of non-pawn material, and mate scores from it are not trusted.
  bool pvNode = beta - alpha > 1;
  if (allowNull && !pvNode && !inCheck && depth >= 3 && ply < MaxPly - 2 && beta < MateScore - MaxPly) {
    int material = 0;
    for (ChessCoord c : b.pieces[b.nextColor]) {
      uint8_t t = b.board[c.coord].type();
      if (t != PIECE_PAWN && t != PIECE_KING) material += kPieceValue[t];
    }
    if (material >= 400 && BoardEval::Eval(b) >= beta) {
      int R = 2 + (depth >= 6 ? 1 : 0) + (depth >= 10 ? 1 : 0);
      bool hadEp = b.hasEnPassant;
      uint64_t savedHash = b.HashVal();
      ChessCoord ep = b.enPassantDst;
      b.hashHistory.push_back(b.HashVal());
      b.hash.NullMove(b);
      b.hasEnPassant = false;
      b.halfMoves++;
      b.ApplyNullMove();
      int score = -Negamax(b, depth - 1 - R, ply + 1, -beta, -beta + 1, false);
      b.UndoNullMove();
      b.halfMoves--;
      b.hasEnPassant = hadEp;
      b.enPassantDst = ep;
      b.hashHistory.pop_back();
      b.hash.hash = savedHash;
      if (stopped_) return 0;
      if (score >= beta) return score >= MateScore - MaxPly ? beta : score;
    }
  }
  OrderMoves(b, moves, ttMove, ply);

  int origAlpha = alpha;
  int best = -2 * MateScore;
  uint16_t bestMove = 0;
  int moveNo = 0;
  for (const Move& m : moves) {
    bool quiet = !IsCapture(b, m) && m.promo.isEmpty();
    bool killer = ply < MaxPly && ((killers_[ply][0].src == m.src && killers_[ply][0].dst == m.dst) ||
                                   (killers_[ply][1].src == m.src && killers_[ply][1].dst == m.dst));
    UndoMove u = b.GetUndoMove(m);
    b.ApplyMove(m);
    int score;
    if (moveNo == 0) {
      score = -Negamax(b, depth - 1, ply + 1, -beta, -alpha);
    } else {
      // late move reduction for quiet moves that are unlikely to matter, then a null-window
      // (principal variation) search; re-search only if it unexpectedly beats alpha
      int reduce = 0;
      if (quiet && !killer && !inCheck && depth >= 3 && moveNo >= 3) {
        reduce = 1 + (moveNo >= 8 && depth >= 6 ? 1 : 0);
      }
      score = -Negamax(b, depth - 1 - reduce, ply + 1, -alpha - 1, -alpha);
      if (score > alpha && reduce) score = -Negamax(b, depth - 1, ply + 1, -alpha - 1, -alpha);
      if (score > alpha && score < beta) score = -Negamax(b, depth - 1, ply + 1, -beta, -alpha);
    }
    moveNo++;
    b.Undo(u);
    if (stopped_) return 0;
    if (score > best) {
      best = score;
      bestMove = Pack(m);
    }
    if (score > alpha) alpha = score;
    if (alpha >= beta) {
      if (quiet && ply < MaxPly) {
        killers_[ply][1] = killers_[ply][0];
        killers_[ply][0] = m;
        int& h = history_[b.nextColor][m.src.coord][m.dst.coord];
        h = std::min(h + depth * depth, 60000);
      }
      break;
    }
  }

  if (e.depth <= depth || e.hash != h) {
    e.hash = h;
    e.score = ToTable(best, ply);
    e.move = bestMove;
    e.depth = int8_t(std::min(depth, 100));
    e.bound = best <= origAlpha ? 1 : (best >= beta ? 2 : 0);
  }
  return best;
}

SearchResult Search::Think(ChessBoard& board, int maxDepth, int timeMs) {
  SearchResult res;
  stopped_ = false;
  timed_ = timeMs > 0;
  nodes_ = 0;
  if (timed_) deadline_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeMs);
  for (auto& k : killers_) k[0] = k[1] = Move();
  for (auto& a : history_) for (auto& r : a) for (int& v : r) v /= 4;

  std::vector<Move> moves = board.GetMoves();
  if (moves.empty()) {
    res.score = board.IsInCheck() ? -MateScore : 0;
    return res;
  }
  // a legal move is always available, even if the clock runs out during depth 1
  res.best = moves[0];
  res.hasMove = true;

  uint16_t prevBest = 0;
  for (int depth = 1; depth <= maxDepth; depth++) {
    OrderMoves(board, moves, prevBest, 0);
    int alpha = -2 * MateScore, beta = 2 * MateScore;
    int iterBest = -2 * MateScore;
    Move iterMove = moves[0];
    for (const Move& m : moves) {
      UndoMove u = board.GetUndoMove(m);
      board.ApplyMove(m);
      int score = -Negamax(board, depth - 1, 1, -beta, -alpha);
      board.Undo(u);
      if (stopped_) break;
      if (score > iterBest) {
        iterBest = score;
        iterMove = m;
      }
      alpha = std::max(alpha, score);
    }
    if (stopped_) {
      res.timedOut = true;
      break;
    }
    res.best = iterMove;
    res.score = iterBest;
    res.depth = depth;
    prevBest = Pack(iterMove);
    if (iterBest > MateScore - MaxPly || iterBest < -MateScore + MaxPly) break;  // forced mate found
  }
  res.nodes = nodes_;
  return res;
}
