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
