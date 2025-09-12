#include "Range.h"

std::vector<Range> divideRange(unsigned nTasks, unsigned nProc) {
  if (nTasks == 0) {
    return {};
  }
  if (nProc == 0) {
    nProc = 1;
  }
  if (nProc > 1000) {
    nProc = 1000;
  }
  unsigned chunkSize = nTasks / nProc + (nTasks % nProc > 0);
  std::vector<Range> chunks;
  for (unsigned i = 0; i < nProc; i++) {
    Range r;
    r.begin() = i * chunkSize;
    r.end() = (i + 1) * chunkSize;
    if (r.end() > nTasks) {
      r.end() = nTasks;
    }
    chunks.push_back(r);
    if (r.end() >= nTasks) {
      break;
    }
  }
  return chunks;
}