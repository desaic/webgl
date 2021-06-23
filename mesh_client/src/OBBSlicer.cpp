#include "OBBSlicer.h"
#include "TetSlicer.h"
#include "TrigIter2D.h"

void SliceOBB(OBBox & obb, const VoxelCb cb)
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
  Vec3f tets[5][4] = { {c0, c4, c5, c6}, {c0, c5, c1, c3}, {c6, c2, c3, c0 }, {c7, c6, c3, c5}, {c6, c5, c0, c3} };
  const unsigned NUM_TETS = 5;
  const unsigned TET_VERTS = 4;
  for (unsigned n = 0; n < NUM_TETS; n++) {
    TetSlicer slicer(tets[n][0], tets[n][1], tets[n][2], tets[n][3]);
    float zmin=tets[n][0][2], zmax = tets[n][0][2];
    for (unsigned i = 1; i < TET_VERTS; i++) {
      zmin = std::min(zmin, tets[n][i][2]);
      zmax = std::max(zmax, tets[n][i][2]);
    }
    //integer values of z min and max
    int k_min = int(zmin);
    int k_max = int(zmax)+1;
    for (int k = k_min; k <= k_max; k++) {
      Vec2f vertices[4];
      float z = (k + 0.5f);
      int numVerts = slicer.intersect(z, vertices);
      if (numVerts < 3) {
        continue;
      }
      int numTrigs = (numVerts == 3) ? 1 : 2;

      for (int c = 1; c <= numTrigs; ++c) {
        TrigIter2D pixel(vertices[0], vertices[c], vertices[c + 1]);
        while (pixel()) {
          int i = pixel.x();
          int j = pixel.y();
          cb(i, j, k);
          ++pixel;
        }
      }
    }
  }
}
