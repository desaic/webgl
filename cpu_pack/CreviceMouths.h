#pragma once

#include "PackingConfig.h"
#include "PackingScene.h"

#include <vector>

/// Find local passage centers between two opposing fruit surfaces, starting
/// from exposed crevice lattice points. Exports points, feasible-width points,
/// a same-pair neighborhood graph, and per-point measurements for Blender.
/// Returns centers wide enough to seed the smallest medium sphere.
std::vector<Vec3f> ExportCreviceMouths(PackingScene &scene, const PackingConfig &cfg,
                                       const std::vector<Vec3f> &targets,
                                       float minMediumRadius);
