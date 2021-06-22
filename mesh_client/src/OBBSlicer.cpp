#include "OBBSlicer.h"

void SliceOBB(OBBox & obb)
{  
  // For each OBB, the 8 corners are numerated as:
  //
  //     2*-----*3        y
  //     /|    /|         ^
  //    / |   / |         |
  //  6*-----*7 |         |
  //   | 0*--|--*1        +--->x
  //   | /   | /         /
  //   |/    |/        |/_
  //  4*-----5         z
  //
  const Vec3f & c0 = obb.origin;
  Vec3f c1 = c0 + obb.axes[0];
  Vec3f c2 = c0 + obb.axes[1];
  Vec3f c3 = c0 + obb.axes[0] + obb.axes[1];
  Vec3f c4 = c0 + obb.axes[2];
  Vec3f c5 = c0 + obb.axes[0] + obb.axes[2];
  Vec3f c6 = c0 + obb.axes[1] + obb.axes[2];
  Vec3f c7 = c0 + obb.axes[0] + obb.axes[1] + obb.axes[2];
  Vec3f Tets[5][4] = { {c0, c4, c5, c6}, {c0, c5, c1, c3}, {c6, c2, c3, c0 }, {c7, c6, c3, c5}, {c6, c5, c0, c3} };
}