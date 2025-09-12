#include <vector>

struct Range {
  unsigned i0 = 0, i1 = 0;
  Range() {}
  Range(unsigned a, unsigned b) : i0(a), i1(b) {}
  unsigned begin() const { return i0; }
  unsigned end() const { return i1; }
  unsigned& begin() { return i0; }
  unsigned& end() { return i1; }
};

std::vector<Range> divideRange(unsigned nTasks, unsigned nProc);