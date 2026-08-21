#pragma once

#include "PackingConfig.h"
#include "PackingPlan.h"
#include "PackingScene.h"

/// loads items and container meshes, inits broad phase, container sdf
/// and acceleration grids. does not voxelize the packing background.
/// benchmarks call this to get a ready scene without packing anything.
/// @return false if the item directory or container mesh is missing.
bool BuildScene(PackingScene &scene, const PackingConfig &cfg);

/// voxelizes the container into the packing occupancy grid and inverts it
/// so that the exterior is unavailable.
void PrepareBackground(PackingScene &scene, const PackingConfig &cfg);

/// runs one step of the plan: repeatedly find a spot and nudge an item in.
void PackStep(PackingScene &scene, const PackingStep &step, const PackingConfig &cfg);

/// prepares the background, optionally resumes from a pack file,
/// then runs plan steps starting at cfg.startStep.
void PackScene(PackingScene &scene, const PackingPlan &plan, const PackingConfig &cfg);

/// end to end: build the scene then pack it.
void PackFruits(const PackingPlan &plan, const PackingConfig &cfg);

/// casts one inward ray per container surface sample point against all
/// placed instances, saves surface_depths.obj and deep_rays.obj under
/// scene.outputFolder, and returns the deep ray origins/ends. does not
/// place or settle anything, so it is safe to call without running any
/// packing steps.
void ComputeAndSaveSurfaceDepths(PackingScene &scene,
                                 std::vector<Vec3f> &deepOrigins,
                                 std::vector<Vec3f> &deepEnds);

/// debug helper: finds the container-surface ray closest to targetPos and
/// prints its depth plus every neighbor ray within the same radius
/// FindDeepRays uses, so a spurious "deep ray" flag on a curved surface
/// can be diagnosed without running the packing steps.
void DebugDeepRayNeighbors(PackingScene &scene, const Vec3f &targetPos);
