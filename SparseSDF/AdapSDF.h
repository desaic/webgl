#ifndef ADAP_SDF_H
#define ADAP_SDF_H

#include "Array3D.h"
#include "SparseNode4.h"
// a grid cell containing sample points.
struct PointSet {
  std::vector<Vec3f> p;
};
/// <summary>
/// stores a dense coarse distance grid
/// and a sparse grid of surface point samples.
/// </summary>
class AdapSDF {
 public:

  AdapSDF();
  /// <summary>
  /// inputs are voxel grid size. vertex grid size would be allocated
  /// to be +1 of voxel grid size.
  /// coarse grid will be ceil(voxel grid size/4).
  /// </summary>
  /// <returns>-1 if too many voxels are requested</returns>
  int Allocate(unsigned sx, unsigned sy, unsigned sz);

  /// <summary>
  ///
  /// </summary>
  /// <param name="point">input is in mesh space. Added point is in local 
  /// grid space.</param>
  /// <returns>0 if point added successfully. negative if point is outside of
  /// coarse grid
  /// or too close to existing sample points</returns>
  int AddPoint(Vec3f point, const Vec3u& gridIdx);

  PointSet& AddSparseCell(const Vec3u& gridIdx);

  /// <summary>
  /// After adding points, compress to save memory.
  /// Dense operations no longer valid after this point.
  /// </summary>
  void Compress();

  /// <summary>
  /// Biliearly interpolates the vertex distance grid.
  /// </summary>
  /// <returns>distance in mm</returns>
  float GetCoarseDist(const Vec3f & x) const;
  /// <summary>
  /// taken as the min of the bilinear interpolated distance
  /// and the point sample based distance.
  /// </summary>
  /// <returns>distance in mm</returns>
  float GetFineDist(const Vec3f& x) const;

  SparseNode4<unsigned>& GetSparseNode4(unsigned x, unsigned y, unsigned z);
  const SparseNode4<unsigned>& GetSparseNode4(unsigned x, unsigned y,
                                              unsigned z) const;
  /// 2 bils.
  static const size_t MAX_NUM_VOX = 1u << 31;

  /// band causes furthur expansion of the grid.
  static const unsigned MAX_BAND = 8;

  // max positive value of short int
  static const short MAX_DIST = 0x7fff;

  // distance values are stored on grid vertices.
  // vertex grid size is voxel grid size +1.
  // stores only 16-bit ints to save memory.
  // distance values won't be too large because of band.
  // physical distance = dist * distUnit.
  Array3D<short> dist;

  // coarse grid contains index into list of refined cells.
  // only a sparse subset of voxels have refined cells.
  // sparse nodes are stored at 1/4 resolution of the full grid.
  Array3D<SparseNode4<unsigned>> sparseGrid;

  // flat list of sparse cell data indexed by sparseGrid.
  // index is reserved for empty cells.
  std::vector<PointSet> sparseData;

  // mm. default is 1um.
  float distUnit = 0.001f;

  Vec3f origin = {0.0f, 0.0f, 0.0f};

  // in mm. square voxels only.
  float voxSize;
  // 1/4 of voxSize by default.
  float pointSpacing = 0.1f;
  unsigned band = 5;

  size_t totalPoints = 0;
};

#endif
