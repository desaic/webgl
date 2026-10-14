#pragma once

#include "PackingConfig.h"
#include "PackingPlan.h"
#include "PackingScene.h"

/// Two-pass crevice fill guided by the coarse container shell and its patches
/// (ContainerShell.h). Pass 1 plugs open holes: blobs of shell rays that hit no
/// fruit within cfg.fillMaxDiag. Pass 2 plugs crevices: blobs of rays whose hit
/// lies deep inside the convex hull of the touching fruit group. Both passes
/// place proxy spheres sized from the 8 to 14 cm fruit group. Only fruit already
/// loaded into the scene (the sphereMaxX subset) are considered.
void RunShellPasses(PackingScene &scene, const PackingStep &medium, const PackingConfig &cfg);
