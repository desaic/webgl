#pragma once

#include "PackingConfig.h"

#include <string>
#include <vector>

/// Cavity refinement of the 4 to 8 cm fruit (see CavityField.h for what a cavity is).
///
/// SOP, repeated cfg.cavPasses times, tile by tile over the container:
///  1. cavities: free space within cfg.cavBand of the wall that a ball of radius cfg.cavR cannot
///     reach, big enough for a ball of cfg.cavMinClear. ranked by volume weighted toward the wall.
///  2. relocate: each existing medium fruit near a cavity is lifted out; poses are tried in the
///     cavities near it (long axis along the cavity axis, a few rolls) and settled with a nudge into
///     the valley. kept only if the weighted cavity volume of the local window drops by more than
///     cfg.cavMinGain versus leaving it where it was. rotations and positions are thus judged by
///     the cavity objective, not by overlap alone.
///  3. spawn: for the largest cavities, kinds that fit the clearance are tried the same way and the
///     best pose is kept if it removes more than cfg.cavMinGain.
/// Large fruit near the wall are never covered: only free space counts, never fruit surface.
/// loads cfg.resumePackFile, writes pack_cavity_<pass>.txt and pack_cavity.txt to the output folder.
bool RunCavityRefine(const PackingConfig &cfg, const std::vector<std::string> &mediumNames, bool metricOnly);
