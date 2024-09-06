#include "Array3DUtils.h"

float TrilinearInterp(const Array3Df& dist, const Vec3f& x, const Vec3f& voxSize) {
  float MAX_DIST = dist.GetSize()[0] * voxSize[0] * 10.0f;
  return TrilinearInterpWithDefault(dist, x, voxSize, MAX_DIST);
}

float TrilinearInterpWithDefault(const Array3Df& dist, Vec3f local, const Vec3f& voxSize,
                                 float defaultVal) {
  local = local / voxSize;
  unsigned ix = unsigned(local[0]);
  unsigned iy = unsigned(local[1]);
  unsigned iz = unsigned(local[2]);
  Vec3u gridSize = dist.GetSize();
  if (ix >= gridSize[0] - 1 || iy >= gridSize[1] - 1 || iz >= gridSize[2] - 1) {
    return defaultVal;
  }
  float a = (ix + 1) - local[0];
  float b = (iy + 1) - local[1];
  float c = (iz + 1) - local[2];
  // v indexed by y and z
  float v00 = a * dist(ix, iy, iz) + (1 - a) * dist(ix + 1, iy, iz);
  float v10 = a * dist(ix, iy + 1, iz) + (1 - a) * dist(ix + 1, iy + 1, iz);
  float v01 = a * dist(ix, iy, iz + 1) + (1 - a) * dist(ix + 1, iy, iz + 1);
  float v11 = a * dist(ix, iy + 1, iz + 1) + (1 - a) * dist(ix + 1, iy + 1, iz + 1);
  float v0 = b * v00 + (1 - b) * v10;
  float v1 = b * v01 + (1 - b) * v11;
  float v = c * v0 + (1 - c) * v1;
  return v;
}
