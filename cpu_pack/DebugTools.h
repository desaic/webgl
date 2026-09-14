#pragma once

#include "MeshInfo.h"
#include "PackingConfig.h"

#include <string>

/// one-off helpers kept out of main.cpp. not part of the packing pipeline.

/// marching cubes an inset surface of the container and saves it.
void MakeInnerMesh(const PackingConfig &cfg, float insetVoxels = 4.0f);

/// dumps sample points before and after moving them inward, plus the
/// item sdf isosurface, for inspecting contact sample coverage.
void DebugPointSampling(MeshInfo &meshInfo, const std::string &outputFolder);

/// drops a single item into the container and saves its trajectory.
void DebugNudge(const PackingConfig &cfg);

/// loads the scene and its resume pack, voxelizes the placed items at
/// voxelSize, and writes the occupancy grid as a cube mesh to
/// <outputFolder>/item_voxels.obj. does not pack anything, so it is the
/// quickest way to check that the voxelization lines up with the item meshes.
void DebugItemVoxels(const PackingConfig &cfg, float voxelSize = 0.25f);

/// loads the scene and its resume pack, shrink-wraps the placed items, and
/// writes <outputFolder>/shrinkwrap_debug.obj alongside item_meshes.obj, so the
/// hull can be checked against the geometry it wraps without running a pack.
void DebugShrinkWrap(const PackingConfig &cfg, float shrinkRadius = 1.0f,
                     float voxelSize = 0.25f);
