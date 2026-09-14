#pragma once

#include "Array3D.h"
#include "TrigMesh.h"
#include <string>

class PackingScene;

/// voxelizes every placed instance into one solid occupancy grid at the given
/// voxel size. only the items are filled, so unlike scene.bg.vox the space
/// outside the container reads as empty. the grid is sized to the union of the
/// instance bounding boxes plus 2 voxels of padding, independent of scene.dx
/// and the scene world origin.
/// @param outOrigin min corner of voxel (0,0,0), same convention as
///                  VoxConf::origin: voxel (i,j,k) covers the cube
///                  [outOrigin + idx*voxelSize, outOrigin + (idx+1)*voxelSize].
Array3D8u VoxelizeItems(PackingScene &scene, float voxelSize, Vec3f &outOrigin);

/// computes a shrinkwrap mesh from the placed instances: voxelizes them,
/// flood-fills outside, shrink-wraps with the given radius, and
/// marching-cubes the result. default radius is 1cm.
/// voxelSize is the shrinkwrap's own grid resolution, independent of scene.dx.
TrigMesh ComputeShrinkWrapMesh(PackingScene &scene, float shrinkRadius = 1.0f,
                               float voxelSize = 0.25f);

/// calls ComputeShrinkWrapMesh and saves the result to an obj file.
void SaveShrinkWrapMesh(PackingScene &scene, const std::string &filename,
                        float shrinkRadius = 1.0f, float voxelSize = 0.25f);
