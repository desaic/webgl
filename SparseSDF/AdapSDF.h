#ifndef ADAP_SDF_H
#define ADAP_SDF_H

#include "Array3D.h"
#include "SparseNode4.h"
#include "Sparse3DMap.h"
// a fixed 5x5x5 grid.
struct FixedGrid5 {
  static const unsigned N = 5;
  static const unsigned LEN = 128;

  // extra bytes for padding
  short val[LEN];

  short operator()(unsigned x, unsigned y, unsigned z) const {
    return val[x + y * N + z * N*N];
  }
  short& operator()(unsigned x, unsigned y, unsigned z) {
    return val[x + y * N + z * N*N];
  }

  void Fill(short v) {
    for (unsigned i = 0; i < LEN; i++) {
      val[i] = v;
    }
  }
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

  FixedGrid5& AddSparseCell(const Vec3u& gridIdx);
  bool HasCellDense(const Vec3u& gridIdx) const;
  bool HasCellSparse(const Vec3u& gridIdx) const;
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
  unsigned GetSparseCellIdx(unsigned x, unsigned y, unsigned z) const;
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
  // 
  // only a sparse subset of voxels have refined cells.
  // sparse nodes are stored at 1/4 resolution of the full grid.
  Array3D<SparseNode4<unsigned>> sparseGrid;

  // temporary list of triangles for each coarse voxel.
  Array3D<SparseNode4<unsigned>> trigList; 

  // flat list of sparse cell data indexed by sparseGrid.
  // index 0 is reserved for empty cells.
  std::vector<FixedGrid5> sparseData;

  // mm. default is 1um.
  float distUnit = 0.001f;

  Vec3f origin = {0.0f, 0.0f, 0.0f};

  // in mm. square voxels only.
  float voxSize;
  // 1/4 of voxSize by default.
  float pointSpacing = 0.1f;
  unsigned band = 5;

};

#endif
