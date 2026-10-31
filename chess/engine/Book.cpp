#include "Book.h"

#include "book_data.h"

#include <algorithm>
#include <chrono>
#include <random>

uint16_t PackMove(const Move& m) {
  return uint16_t(m.src.coord | (m.dst.coord << 6) | ((m.promo.info & 15) << 12));
}

static unsigned Key(const ChessBoard& b) {
  uint64_t h = b.HashVal();
  return unsigned(h >> 32) ^ unsigned(h);
}

// range [first, last) of the entries for this position
static void Range(const ChessBoard& b, size_t& first, size_t& last) {
  const unsigned key = Key(b);
  const unsigned* begin = kBookKeys;
  const unsigned* end = kBookKeys + kBookCount;
  first = std::lower_bound(begin, end, key) - begin;
  last = std::upper_bound(begin, end, key) - begin;
}

bool BookPick(const ChessBoard& b, const std::vector<Move>& legal, BookChoice& out) {
  size_t first, last;
  Range(b, first, last);
  struct Cand {
    const Move* move;
    int weight;
  };
  std::vector<Cand> cands;
  int total = 0;
  for (size_t i = first; i < last; i++) {
    const uint16_t packed = uint16_t(kBookMoves[i] & 0xffff);
    const int weight = int(kBookMoves[i] >> 16);
    // a key collision could name a move that is not legal here: ignore those
    for (const Move& m : legal) {
      if (PackMove(m) == packed) {
        cands.push_back({&m, weight});
        total += weight;
        break;
      }
    }
  }
  if (cands.empty()) return false;
  static std::mt19937 rng(unsigned(std::chrono::steady_clock::now().time_since_epoch().count()));
  int r = int(rng() % unsigned(total));
  for (const Cand& c : cands) {
    if (r < c.weight) {
      out.move = *c.move;
      out.games = c.weight;
      return true;
    }
    r -= c.weight;
  }
  return false;
}

bool BookHasMove(const ChessBoard& b, uint16_t move) {
  size_t first, last;
  Range(b, first, last);
  for (size_t i = first; i < last; i++) {
    if (uint16_t(kBookMoves[i] & 0xffff) == move) return true;
  }
  return false;
}

unsigned BookSize() { return kBookCount; }
