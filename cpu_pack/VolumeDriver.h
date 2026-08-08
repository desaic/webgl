#pragma once

#include "PocketVolume.h"

#include <string>

class PackingScene;
struct PackingStep;

// H7 loop skeleton for the final step, parallel to PocketDriver.h's
// ray/patch version: pockets are free-space connected components instead of
// surface rays, so a component's own local-max-depth point is the natural
// placement target instead of a point along a ray. Reuses PackingScene's
// Nudge/Put exactly as PocketDriver does; only the targeting and
// accept/reject geometry differ, since there is no ray line to measure
// lateral/along-ray drift against.
struct VolumeStepConfig {
  PocketVolumeParams volume;

  // same safety-backstop framing as PocketStepConfig: the real "done"
  // signal is running out of components at or above minTargetDepth.
  float maxSecondsPerStep = 60.0f;
  unsigned maxBerries = 1000000u;

  unsigned pocketTrialCount = 3;
  float searchSlack = 1.0f;
  // no kind may take more than this share of placements once a few have
  // landed, so size matching can't silently collapse onto one kind.
  float kindShareCap = 0.25f;
};

struct VolumeStepResult {
  unsigned placed = 0;
  unsigned attempts = 0;
  // see PocketStepResult::bootstrapPlacements -- same bootstrap exception,
  // always 0 in production.
  unsigned bootstrapPlacements = 0;
  unsigned rejectedNoContact = 0;
  unsigned rejectedTooFar = 0;
  std::string exitReason;
  // final volumetric state, for the caller to report/dump.
  PocketVolumeField field;
};

VolumeStepResult PackVolumeStep(PackingScene &scene, const PackingStep &step,
                                const VolumeStepConfig &cfg);
