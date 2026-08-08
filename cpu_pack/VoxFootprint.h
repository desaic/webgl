#pragma once

#include "Array3D.h"

#include <cstdint>
#include <unordered_map>

class TrigMesh;

// voxelization of one item kind at one rotation, at a given dx. Cached
// because the pocket local search (FindSpotLocal, PackingOpsLocal.cpp)
// tests the same (kind, rotation) footprint against ~1700 candidate
// offsets, and improvement_plan.md 5.4 wants the same cache for its
// fg_voxelize path.
//
// vox is the voxelization of the ROTATED-ONLY mesh (no translation), so it
// is reusable against any candidate world position. That is only valid
// because every candidate a caller tests is itself an exact multiple of dx
// from world zero -- the same lattice bg/classGrid's own origin is aligned
// to (AlignOriginToGrid, GridUtils.cpp) -- so aligning the footprint's own
// bbox to that lattice and later adding a lattice-aligned candidate
// position commute. A candidate off the lattice would silently misalign
// the footprint by up to half a voxel.
struct VoxFootprint {
  Array3D8u vox; // 1 occupied, 0 free.
  // world offset from a candidate placement position to vox(0,0,0):
  // world position of vox(i,j,k) when placed at candidate p is
  // p + localOrigin + dx*(i,j,k).
  Vec3f localOrigin;

  size_t MemoryBytes() const { return vox.GetData().size(); }
};

using VoxFootprintCache = std::unordered_map<uint64_t, VoxFootprint>;

inline uint64_t VoxFootprintKey(unsigned kindId, unsigned rotIdx) {
  return (uint64_t(kindId) << 32) | uint64_t(rotIdx);
}

// returns the cached footprint for (kindId, rotIdx), building it from part
// rotated by rot if this is the first request for that key. rotIdx is a
// caller-assigned stable index into a fixed rotation set (e.g. an index
// into PackingScene::randAngles) -- callers must not vary rot for a given
// rotIdx, or the cache silently returns a stale footprint.
const VoxFootprint &GetOrBuildFootprint(VoxFootprintCache &cache, unsigned kindId,
                                        unsigned rotIdx, const TrigMesh &part,
                                        const Vec3f &rot, float dx);

size_t VoxFootprintCacheBytes(const VoxFootprintCache &cache);
