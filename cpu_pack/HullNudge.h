#pragma once

#include "HullSet.h"
#include "PackingScene.h"

#include <functional>
#include <vector>

struct HullNudgeStats {
  unsigned steps = 0;
  unsigned long pointTests = 0;  // sample x neighbor distance queries
  unsigned long contacts = 0;    // contacts before the manifold reduction
  double contactMs = 0.0;        // time in contact gathering
  double totalMs = 0.0;
};

/// rigid body nudge of one fruit without voxels or triangle grids. the fruit is its hull set
/// samples, its neighbors are their hull sets (planes), the container wall is the container
/// TrigGrid. the contact solver is the one PackingScene::Nudge uses. forceField is evaluated at
/// every sample each step and summarized into one linear acceleration (mean force) and one
/// angular acceleration (mean torque about the center of mass), so a fruit stuck at one end can
/// rotate. the fruit's own instance must not be in the broad phase. returns the final pose.
RigidTransform NudgeWithHulls(PackingScene &scene, HullLibrary &lib, unsigned itemIdx,
                              const RigidTransform &tran,
                              const std::function<Vec3f(const Vec3f &)> &forceField,
                              size_t maxSteps, std::vector<RigidTransform> &trajectory,
                              HullNudgeStats *stats = nullptr);
