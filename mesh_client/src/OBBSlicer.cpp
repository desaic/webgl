#include "OBBSlicer.h"
#include "TetSlicer.h"
#include "TrigIter2D.h"
#include <fstream>
int obb_ceil(float f){
  int i = int(f);
  if (i == f) {
    return i;
  }
  if (f < 0) {
    return i;
  }
  else {
    return i + 1;
  }
}
static void SavePointsToObj(const std::string& filename, const std::vector<Vec3f>& points)
{
  std::ofstream out(filename);
  for (size_t i = 0; i < points.size(); i++) {
    out << "v " << points[i][0] << " " << points[i][1] << " " << points[i][2] << "\n";
  }
  out.close();
}

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
  for (unsigned i = 1; i < NUM_BOX_VERTS; i++) {
    boxZMin = std::min(c[i][2], boxZMin);
    boxZMax = std::max(c[i][2], boxZMax);
  }
  int boxKMin = obb_ceil(boxZMin);
  int boxKMax = int(boxZMax);
  const unsigned NUM_TETS = 5;
  const unsigned TET_VERTS = 4;
  voxels.zmin = boxKMin;
  voxels.slices.resize( size_t(boxKMax - boxKMin + 1) );
  for (unsigned n = 0; n < NUM_TETS; n++) {
    TetSlicer slicer(tets[n][0], tets[n][1], tets[n][2], tets[n][3]);
    float zmin = tets[n][0][2], zmax = tets[n][0][2];
    for (unsigned i = 1; i < TET_VERTS; i++) {
      zmin = std::min(zmin, tets[n][i][2]);
      zmax = std::max(zmax, tets[n][i][2]);
    }
    //integer values of z min and max
    int k_min = obb_ceil(zmin);
    int k_max = int(zmax);
    for (int k = k_min; k <= k_max; k++) {
      Vec2f vertices[4];
      float z = k;
      int numVerts = slicer.intersect(z, vertices);
      if (numVerts < 3) {
        continue;
      }

      int numTrigs = (numVerts == 3) ? 1 : 2;
      float fymin=vertices[0][1], fymax=vertices[0][1];
      for (int vi = 1; vi < numVerts; vi++) {
        if (fymin > vertices[vi][1]) {
          fymin = vertices[vi][1];
        }
        if (fymax < vertices[vi][1]) {
          fymax = vertices[vi][1];
        }
      }
      int ymin = obb_round(fymin);
      int ymax = obb_ceil(fymax);

      for (int ti = 1; ti <= numTrigs; ++ti) {
        SparseSlice<int> slice;
        slice.ymin = ymin;
        slice.rows.resize( size_t(ymax - ymin + 1), Interval<int> ( 0,-1));
        
        TrigIter2D pixel(vertices[0], vertices[ti], vertices[ti + 1]);
        while (pixel()) {
          Interval<int> row(pixel.x0(), pixel.x1());
          int y = pixel.y();
          if (y < ymin) {
            //should never happen.
            y = ymin;
          }
          size_t yIdx = size_t(y - ymin);
          if (slice.rows[yIdx].IsEmpty()) {
            slice.rows[yIdx]=row;
          }
          else {
            slice.rows[yIdx].Expand(row);
          }
          ++pixel;
        }
        size_t zIdx = size_t(k - boxKMin);
        voxels.slices[zIdx].Expand(slice);
      }
    }
  }
}

int obb_round(float f)
{
  if (f >= 0) {
    return int(f + 0.5f);
  }
  else {
    return int(f - 0.5f);
  }
}