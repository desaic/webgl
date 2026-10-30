#pragma once

#include "Array3D.h"
#include "MeshConvo.h"
#include "MeshInfo.h"

#include <vector>

/// distance in voxels from every voxel to the nearest voxel with source != 0 (exact EDT).
Array3D<float> DistanceToSource(const Array3D8u &source);

/// radius (cm) of the largest sphere that fits inside the item mesh, from an exact
/// distance transform of its voxelization at voxel size dx.
float InscribedRadius(const MeshInfo &item, float dx = 0.5f);

/// Upper bound on the free space left in each subgrid cell, used to skip cells in which a
/// fruit cannot fit: the empty voxels of the background can add up to a fruit's volume while
/// being spread over thin gaps no fruit fits in.
///
/// The background is coarsened by `factor`; a coarse voxel counts as occupied only if all of
/// its voxels are, so free space is over-estimated and a feasible spot is never rejected.
/// The clearance is the distance to the nearest occupied coarse voxel, taken per cell as the
/// maximum over the cell box grown by `reach` cm (a fruit placed in a cell extends past it).
struct ClearanceGate {
  unsigned factor = 4;
  float reach = 14.0f;  // cm, the largest fruit extent
  unsigned version = ~0u;
  std::vector<float> cellMax;  // cm, per subgrid cell index
  float coarseDx = 0.0f;

  /// recomputes cellMax from bg (the full-resolution grid; never FFT'd here).
  void Rebuild(const MeshConvo &bg, float cellSize, const Vec3u &numCells);

  /// true if the cell may hold a fruit whose inscribed radius is rIn. slack covers the
  /// background erosion and the coarse quantization.
  bool MayFit(unsigned cellIdx, float rIn) const {
    const float need = rIn - 1.5f - coarseDx;
    return need <= 0.0f || cellMax[cellIdx] >= need;
  }
};
