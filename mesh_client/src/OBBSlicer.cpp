#include "OBBSlicer.h"
#include "TetSlicer.h"
#include "TrigIter2D.h"

void OBBSlicer::Compute(OBBox& obb, SparseVoxel<int>& voxels)
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
  Vec3f c[8];
  c[0] = obb.origin;
  c[1] = c[0] + obb.axes[0];
  c[2] = c[0] + obb.axes[1];
  c[3] = c[0] + obb.axes[0] + obb.axes[1];
  c[4] = c[0] + obb.axes[2];
  c[5] = c[0] + obb.axes[0] + obb.axes[2];
  c[6] = c[0] + obb.axes[1] + obb.axes[2];
  c[7] = c[0] + obb.axes[0] + obb.axes[1] + obb.axes[2];
  Vec3f tets[5][4] = { {c[0], c[4], c[5], c[6]}, {c[0], c[5], c[1], c[3]}, 
    {c[6], c[2], c[3], c[0] }, {c[7], c[6], c[3], c[5]}, {c[6], c[5], c[0], c[3]} };

  float boxZMin = c[0][2];
  float boxZMax = c[0][2];
  const unsigned NUM_BOX_VERTS = 8;
  for (unsigned i = 0; i < NUM_BOX_VERTS; i++) {
    boxZMin = std::min(c[i][2], boxZMin);
    boxZMax = std::max(c[i][2], boxZMax);
  }
  int boxKMin = int(boxZMin);

  const unsigned NUM_TETS = 5;
  const unsigned TET_VERTS = 4;
  for (unsigned n = 0; n < NUM_TETS; n++) {
    TetSlicer slicer(tets[n][0], tets[n][1], tets[n][2], tets[n][3]);
    float zmin = tets[n][0][2], zmax = tets[n][0][2];
    for (unsigned i = 1; i < TET_VERTS; i++) {
      zmin = std::min(zmin, tets[n][i][2]);
      zmax = std::max(zmax, tets[n][i][2]);
    }
    //integer values of z min and max
    int k_min = int(zmin);
    int k_max = int(zmax) + 1;
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
          Interval row(pixel.x0(), pixel.x1());
          int y = pixel.y();

          ++pixel;
        }
      }
    }
  }
}
