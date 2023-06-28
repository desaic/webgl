#ifndef ADAP_SDF_H
#define ADAP_SDF_H

#include "Array3D.h"
#include "SparseNode4.h"

class AdapSDF {
 public:
  /// <summary>
  /// inputs are voxel grid size. vertex grid size would be allocated
  /// to be +1 of voxel grid size.
  /// coarse grid will be ceil(voxel grid size/4).
  /// </summary>
  /// <returns>-1 if too many voxels are requested</returns>
  int Allocate(unsigned sx, unsigned sy, unsigned sz);

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
  Array3D<SparseNode4<unsigned>> coarseGrid;

  // mm. default is 1um.
  float distUnit = 0.001f;

  Vec3f origin = {0.0f, 0.0f, 0.0f};

  // in mm
  Vec3f voxSize;

  unsigned band = 5;
};


#endif
