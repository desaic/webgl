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

/// runs one FillMode::FillSurface step: PocketDriver's pocket-targeted pass
/// (heuristic_plan.md H2 onward) instead of PackStep's lattice walk.
void PackSurfaceStep(PackingScene &scene, const PackingStep &step, const PackingConfig &cfg);

/// prepares the background, optionally resumes from a pack file,
/// then runs plan steps starting at cfg.startStep.
void PackScene(PackingScene &scene, const PackingPlan &plan, const PackingConfig &cfg);

/// end to end: build the scene then pack it.
void PackFruits(const PackingPlan &plan, const PackingConfig &cfg);
