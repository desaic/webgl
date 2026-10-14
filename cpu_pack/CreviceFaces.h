#pragma once

#include "PackingConfig.h"
#include "PackingScene.h"

/// Export short hull-face patches supported by free-space crevice targets.
/// Tight fruit neighbors form triangle and chordless-quad groups; sparse wider
/// neighbors also form pair groups. Patches are screened individually for
/// obstacles and pocket depth, then selected by new target coverage.
void ExportCreviceFaces(PackingScene &scene, const PackingConfig &cfg,
                        const std::vector<Vec3f> &targets);
