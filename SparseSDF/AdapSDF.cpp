#include "AdapSDF.h"

int AdapSDF::Allocate(unsigned sx, unsigned sy, unsigned sz) {
  size_t numVox = sx * sy * size_t(sz);
  if (numVox > MAX_NUM_VOX) {
    return -1;
  }
  dist.Allocate(sx + 1, sy + 1, sz + 1);
  dist.Fill(MAX_DIST);

  Vec3u coarseSize(sx / 4, sy / 4, sz / 4);
  coarseSize[0] += (sx % 4 > 0);
  coarseSize[1] += (sy % 4 > 0);
  coarseSize[2] += (sz % 4 > 0);

  coarseGrid.Allocate(coarseSize[0], coarseSize[1], coarseSize[2]);
  return 0;
}