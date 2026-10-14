#pragma once
#include <vector>
#include "TrigMesh.h"
#include "MeshInfo.h"
#include "Matrix3f.h"
#include "Array3D.h"
#include "cpu_voxelizer.h"
#include "RigidTransform.h"

TrigMesh MakeTransformedMesh(const TrigMesh & mesh, const RigidTransform & tran);

TrigMesh MakeMergedMesh(const TrigMesh & item, const std::vector<RigidTransform> & trans);

Matrix3f RotationMatrixRad(float rx, float ry, float rz);
void TransformVerts(const std::vector<float> &verts, std::vector<float> &dst, const Matrix3f mat);

void VoxelizeMesh(const TrigMesh &m, Array3D8u &grid, VoxConf conf);

// Returns the inner offset surface of mesh at the given offset distance,
// computed via SDF marching cubes. h is the voxel size for SDF computation.
TrigMesh ComputeInnerSurface(TrigMesh &mesh, float h, float offset);