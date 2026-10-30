#pragma once

#include "Array3D.h"
#include "PackingConfig.h"
#include "PackingScene.h"

#include <memory>
#include <string>
#include <vector>

class AdapSDF;

/// per-fruit local force field toward the inner container. a voxel grid around one instance
/// marks the voxels free of every other instance and the exclusion mesh (the fruit itself is
/// not an obstacle; the outer container is ignored). a multi-source Dijkstra from the free voxels
/// inside the inner container, through free voxels only, gives the path length to the inner
/// container; the force at a voxel points down that distance. blocked voxels have no force.
struct InnerForceField {
  Vec3f origin;
  float dx = 0.5f;
  Array3D<float> dist;      // path length (cm) to the inner container, 1e30 if unreachable
  Array3D8u allowed;        // 1 for free voxels (no other fruit, exclusion mesh or container wall)
  Vec3u start;              // voxel of the instance center of mass
  bool startInside = false; // center of mass already inside the inner container
  unsigned targets = 0;

  Vec3f VoxelCenter(unsigned i, unsigned j, unsigned k) const {
    return origin + dx * Vec3f(i + 0.5f, j + 0.5f, k + 0.5f);
  }
  /// unit direction at the voxel nearest p, zero if unreachable or at a target.
  Vec3f Force(const Vec3f &p) const;
  Vec3f ForceAt(unsigned i, unsigned j, unsigned k) const;
};

/// builds the field for scene.instances[instId]. innerSdf: sdf of the inner container
/// (negative inside).
InnerForceField BuildInnerForceField(PackingScene &scene, unsigned instId, const AdapSDF &innerSdf,
                                     float dx, float margin);

/// every `stride` voxel with a force as a short line, plus the shortest path from the start.
void SaveInnerForceObj(const InnerForceField &f, const std::string &filename, unsigned stride,
                       float lineLen);

/// loads the resume pack, finds the leftmost instance of cfg.forceKind and saves its field to
/// <output>/inner_force_<kind>.obj. true on success.
/// batch: nudges every fruit with center of mass x < cfg.sphereMaxX toward the inner container,
/// nearest first, for cfg.forcePasses passes; saves pack_inner_nudged.txt and traj_inner_nudged.txt.
bool RunInnerForce(const PackingConfig &cfg, bool batch = false);

/// saves the voxels inside the inner container, outside every fruit, with x < cfg.sphereMaxX as a
/// blocky mesh to <output>/inner_free_voxels.obj.
bool RunInnerFree(const PackingConfig &cfg);

/// compares the hull box of every kind with the box of its inertia frame mesh (cfg.hullDir).
bool RunHullCheck(const PackingConfig &cfg);

/// penetration between the hull sets of two instances of a pack file, found by name and nearest position.
bool RunSdfCheck(const PackingConfig &cfg, const std::string &packFile, const std::string &nameA,
                 const Vec3f &posA, const std::string &nameB, const Vec3f &posB);

/// penetration of the given instances into their neighbors in each pack file, by mesh sdf and by hulls.
bool RunDeepCompare(const PackingConfig &cfg, const std::vector<std::string> &packs,
                    const std::vector<unsigned> &ids, unsigned watermelon);

/// removes the fruit of packIn after the resume pack's fruit that do not touch (surface gap <= tol cm,
/// by the mesh sdf) at least minTouch of the resume pack's fruit. writes packOut.
bool RunFilterTouch(const PackingConfig &cfg, const std::string &packIn, const std::string &packOut, float tol,
                    unsigned minTouch);

bool RunHullPair(const PackingConfig &cfg, const std::string &packFile, const std::string &nameA,
                 const Vec3f &posA, const std::string &nameB, const Vec3f &posB);
