#ifndef CPU_VOXELIZER_H
#define CPU_VOXELIZER_H

#include <Vec3.h>
#include <functional>
#include "Array3D.h"
class TrigMesh;

struct VoxConf {
    Vec3f origin;
    Vec3f unit;
    Vec3u gridSize;
};

using VoxCallback = std::function<void(unsigned int, unsigned int, unsigned int, unsigned int)>;

// ignores voxels with negative indices.
// mesh should be placed above conf.origin.
void cpu_voxelize_mesh_cb(VoxConf conf, const TrigMesh *mesh, const VoxCallback &cb);

// set val to 1 for mesh voxels
void cpu_voxelize_grid(VoxConf conf, const TrigMesh *mesh, Array3D8u & grid);

#endif
