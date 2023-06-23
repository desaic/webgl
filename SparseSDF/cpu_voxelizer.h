#ifndef CPU_VOXELIZER_H
#define CPU_VOXELIZER_H

#include "TrigMesh.h"

struct voxconf {
  Vec3f origin;
  Vec3f unit;
  Vec3u gridSize;
};

struct VoxCallback {
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) {}
};

// ignores voxels with negative indices.
// mesh should be placed above conf.origin.
void cpu_voxelize_mesh(voxconf conf, const TrigMesh* mesh,
                       VoxCallback& cb);

#endif