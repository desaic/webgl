#pragma once

#include "SurfaceCoverage.h"

#include <deque>
#include <unordered_map>
#include <vector>

// H2/H4 policy over a CoverageField's patches: which patch to visit next,
// which ray in it to target, and which kind to place there. Holds no
// geometry queries itself (SurfaceCoverage already computed rays/patches);
// this is bookkeeping and ranking only.

struct PocketPlannerParams {
  // fails accumulated since this patch's last success, cumulative across
  // passes (RecordSuccess is the only reset). Once it reaches this, the
  // patch is excluded from every future pass, not just the current one --
  // a real permanent retirement. That is deliberate: it is what makes "all
  // patches exhausted" (NextPatch returning -1 after a rebuild) mean "every
  // deep pocket is filled or confirmed unfillable," the H4 stop condition.
  // Set higher than a first guess would suggest, since this is now the
  // primary thing standing between "gave up too early" and "done."
  unsigned patchMaxFails = 6;
  // a patch is only worth visiting at all if some ray in it is open past
  // this -- a shallow slope this side of the threshold is fine to leave as
  // terrain (easy to 3D print or mill), not a pocket that needs filling.
  // Distinct from (and larger than) SurfaceCoverage's holeDepth, which
  // gates the finer-grained "is this ray touching anything yet" used for
  // rim/mouth-diameter targeting within a patch that already cleared this.
  float deepPocketThreshold = 3.0f;
  // small-kind size range this planner matches mouths against, cm.
  float smallMin = 0.85f;
  float smallMax = 3.0f;
  // no kind may take more than this share of placements once a few have
  // landed, so size matching can't silently collapse onto one kind.
  float kindShareCap = 0.25f;
  unsigned priorityTiers = 5;
};

struct PocketPatchState {
  unsigned consecutiveFails = 0;
  unsigned berriesPlaced = 0;
};

// one small kind's identity and size, as PocketDriver reads them off
// PackingScene -- PocketPlanner does not depend on PackingScene itself.
struct PocketKindInfo {
  unsigned itemIndex;
  float maxExtent;
};

class PocketPlanner {
public:
  PocketPlanner(const CoverageField &field, const PocketPlannerParams &params,
               std::vector<PocketKindInfo> smallKinds);

  // -1 once every eligible patch has had its one attempt this pass; call
  // again to start the next pass (re-tiers from the field's current state).
  int NextPatch();

  // rim ray with the greatest depth in patchId; the deepest interior ray
  // if the patch has open rays but no rim ray; -1 if none are open.
  int PickTargetRay(unsigned patchId) const;

  // largest small kind whose maxExtent fits the target ray's estimated
  // mouth diameter (distance to the nearest covered neighbor, doubled,
  // clamped to [smallMin, smallMax]), tied-broken by least used so far and
  // capped by kindShareCap. -1 if smallKinds is empty.
  int PickKind(unsigned targetRayIdx) const;

  // must be called once per attempt at the patch NextPatch just returned.
  void RecordFail(unsigned patchId);
  void RecordSuccess(unsigned patchId, unsigned kindItemIndex);

  const PocketPatchState &State(unsigned patchId) const { return state_[patchId]; }

private:
  const CoverageField &field_;
  PocketPlannerParams params_;
  std::vector<PocketPatchState> state_;
  std::vector<PocketKindInfo> kinds_;
  std::unordered_map<unsigned, unsigned> kindUseCount_;
  unsigned totalPlaced_ = 0;

  std::deque<unsigned> order_;
  void RebuildOrder();
};
