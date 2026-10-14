#pragma once

#include "PackingConfig.h"
#include "PackingScene.h"
#include "ContainerPatches.h"
#include "TrigMesh.h"

#include <vector>

/// Low-poly approximation of the outer container surface: signed distance
/// field of the container (unsigned distance, outside flood filled), marching
/// cubes at cfg.shellOutset cm outside the surface, then quadric
/// simplification. Normals point away from the container interior.
TrigMesh ComputeContainerShell(PackingScene &scene, const PackingConfig &cfg);

/// Shell plus its overlapping triangle patches (patchRadius, patchAngle).
TrigMesh ComputeContainerPatches(PackingScene &scene, const PackingConfig &cfg,
                                 std::vector<ShellPatch> &patches);

/// Builds the shell and writes it to <output>/container_shell.obj. No-op when
/// cfg.shellVoxel <= 0. Also writes container_patches.obj.
void ExportContainerShell(PackingScene &scene, const PackingConfig &cfg);
