#ifndef CPU_VOXELIZER_H
#define CPU_VOXELIZER_H

#include "TrigMesh.h"
#include "Array3D.h"

struct voxconf {
	Vec3f origin;
	Vec3f unit;
	Vec3u gridSize;
};

void cpu_voxelize_mesh(voxconf conf, const TrigMesh* mesh, Array3D8u & grid);

#endif