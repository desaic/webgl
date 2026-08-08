#pragma once

#include "PocketPlanner.h"
#include "SurfaceCoverage.h"

#include <string>

class PackingScene;
struct PackingStep;

// H2/H3 loop skeleton for the final step only: pockets, not kinds and
// cells, drive placement. Independent of PackingStep::fillMode
// (heuristic_plan.md H5's job) -- callers invoke this directly; PackStep's
// lattice-walk loop is untouched.
struct PocketStepConfig {
  CoverageParams coverage;
  PocketPlannerParams planner;

  // H4 stop: the real "done" signal is PocketPlanner running out of
  // eligible patches (every deep pocket -- see
  // PocketPlannerParams::deepPocketThreshold -- is filled or has exhausted
  // patchMaxFails attempts). A global open-fraction target or a
  // diminishing-returns rate would stop early relative to that: a slow
  // tail of hard-but-fillable pockets is still worth finishing, and a
  // shallow-slope-heavy container can sit at a nonzero open fraction
  // forever without that meaning anything is left to do. maxSecondsPerStep
  // and maxBerries stay as pure safety backstops, not intended targets.
  float maxSecondsPerStep = 60.0f;
  unsigned maxBerries = 1000000u;

  // berries are near-isotropic, so far fewer trials than the lattice
  // walk's maxTrialCount -- confirm against the placement rate rather than
  // assuming, per heuristic_plan.md H2.
  unsigned pocketTrialCount = 3;
  // search box half-extent beyond the item's own size, cm.
  float searchSlack = 1.0f;
};

struct PocketStepResult {
  unsigned placed = 0;
  unsigned attempts = 0;
  // placements accepted on container contact alone because the scene had
  // no items in it yet at all -- see the bootstrap note in PocketDriver.cpp.
  // Always 0 in production, where this step always resumes a container
  // already full of bigger fruit; nonzero only means this step was run
  // against a genuinely empty container (e.g. a small dev container).
  unsigned bootstrapPlacements = 0;
  unsigned rejectedNoContact = 0;
  unsigned rejectedInwardSink = 0;
  unsigned rejectedLeftPatch = 0;
  unsigned rejectedStillOpen = 0;
  std::string exitReason;
  // final coverage state, for the caller to report/dump.
  CoverageField field;
};

PocketStepResult PackPocketStep(PackingScene &scene, const PackingStep &step,
                                const PocketStepConfig &cfg);
