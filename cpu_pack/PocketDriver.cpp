#include "PocketDriver.h"

#include "Log.h"
#include "MeshInfo.h"
#include "MeshOps.h"
#include "PackingOps.h"
#include "PackingPlan.h"
#include "PackingScene.h"
#include "Profiler.h"
#include "Stopwatch.h"
#include "VoxFootprint.h"

#include <algorithm>
#include <cmath>

namespace {

// H3, revised: the container is a reference boundary for the final
// printed/milled sculpture, not a real surface the result has to hug, so
// pushing outward against it is exactly wrong -- it produces berries
// resting only on a wall that will not exist in the physical part, which
// is indistinguishable from floating. What a placed berry actually needs
// is contact with *other placed items*, so push toward whichever nearby
// real material (an item, never the container) is closest: the shallowest
// covered neighbor's actual obstruction point if one exists, else this
// ray's own current obstruction, else nothing (let Nudge's existing
// contact-seeking dynamics take over with no strong bias).
Vec3f PocketPushDirection(const CoverageField &field, unsigned targetRayIdx) {
  const SurfaceRay &ray = field.rays[targetRayIdx];
  auto isItemHit = [](const SurfaceRay &r) {
    return r.hitClass == VOX_ITEM_BIG || r.hitClass == VOX_ITEM_SMALL;
  };

  float shallowestDepth = ray.depth;
  Vec3f target;
  bool found = false;
  if (targetRayIdx < field.rayNeighbors.size()) {
    for (unsigned j : field.rayNeighbors[targetRayIdx]) {
      const SurfaceRay &nb = field.rays[j];
      if (!nb.valid || !isItemHit(nb)) {
        continue;
      }
      if (nb.depth < shallowestDepth) {
        shallowestDepth = nb.depth;
        target = nb.hitPoint;
        found = true;
      }
    }
  }
  if (!found && isItemHit(ray)) {
    target = ray.hitPoint;
    found = true;
  }
  if (!found) {
    return Vec3f(0.0f);
  }
  Vec3f dir = target - ray.mouth;
  if (dir.norm() < 1e-6f) {
    return Vec3f(0.0f);
  }
  dir.normalize();
  return dir;
}

} // namespace

PocketStepResult PackPocketStep(PackingScene &scene, const PackingStep &step,
                                const PocketStepConfig &cfg) {
  PROFILE_SCOPE("pocket.total");
  PocketStepResult result;

  CoverageField field = BuildCoverageField(scene.container.mesh, scene.classGrid,
                                           scene.WorldOrigin(), cfg.coverage);

  std::vector<PocketKindInfo> kinds;
  for (const std::string &name : step.names) {
    unsigned itemIndex = scene.GetItemIndex(name);
    if (itemIndex >= scene.items.size()) {
      continue;
    }
    const MeshInfo &item = scene.items[itemIndex];
    Vec3f ext = item.box.vmax - item.box.vmin;
    float maxExtent = std::max({ext[0], ext[1], ext[2]});
    kinds.push_back(PocketKindInfo{itemIndex, maxExtent});
  }
  if (kinds.empty()) {
    result.exitReason = "no small kinds in step";
    result.field = std::move(field);
    return result;
  }

  PocketPlanner planner(field, cfg.planner, kinds);
  VoxFootprintCache footprintCache;

  Utils::Stopwatch stepClock;
  stepClock.Start();

  while (true) {
    if (stepClock.ElapsedMS() > 1000.0f * cfg.maxSecondsPerStep) {
      result.exitReason = "time budget (safety backstop, not a target)";
      break;
    }
    if (result.placed >= cfg.maxBerries) {
      result.exitReason = "max berries (safety backstop, not a target)";
      break;
    }

    // the real "done" signal: no patch has a deep pocket left to fill.
    // NextPatch() only returns -1 once every patch is either shallow
    // (PocketPlannerParams::deepPocketThreshold) or has exhausted
    // patchMaxFails, so this is "every deep pocket is filled or confirmed
    // unfillable," not an arbitrary rate or percentage.
    int patchIdI = planner.NextPatch();
    if (patchIdI < 0) {
      result.exitReason = "all deep pockets filled or unfillable";
      break;
    }
    unsigned patchId = unsigned(patchIdI);

    int rayIdxI = planner.PickTargetRay(patchId);
    if (rayIdxI < 0) {
      planner.RecordFail(patchId);
      continue;
    }
    unsigned rayIdx = unsigned(rayIdxI);

    int kindIdxI = planner.PickKind(rayIdx);
    if (kindIdxI < 0) {
      planner.RecordFail(patchId);
      continue;
    }
    unsigned kindItemIndex = unsigned(kindIdxI);

    result.attempts++;
    MeshInfo &item = scene.items[kindItemIndex];
    Vec3f itemExtent = item.box.vmax - item.box.vmin;
    float itemMaxExtent = std::max({itemExtent[0], itemExtent[1], itemExtent[2]});

    // aim near the far end of the ray's open run, not its mouth. The mouth
    // sits ~1 cm inside the surface (SurfaceCoverage's berryRadius+dx); a
    // search box and accept window sized to the item's own extent around
    // that point confines every placement to within ~2 cm of the container
    // skin no matter how deep the ray's open run measures -- this was the
    // literal mechanism clustering berries at the boundary while deeper
    // pockets went untouched. Back off from the ray's hit point by half
    // the item so the aim point does not sit inside whatever the ray hit,
    // but never aim shallower than the mouth for a pocket too shallow to
    // need this at all.
    const SurfaceRay &targetRay = field.rays[rayIdx];
    float mouthDepth = (targetRay.mouth - targetRay.origin).dot(targetRay.dir);
    float hitDepth = (targetRay.hitPoint - targetRay.origin).dot(targetRay.dir);
    float standoff = 0.5f * itemMaxExtent + cfg.searchSlack;
    float aimDepth = std::max(mouthDepth, hitDepth - standoff);
    Vec3f targetPoint = targetRay.origin + aimDepth * targetRay.dir;

    Box3f searchBox;
    Vec3f half = 0.5f * itemExtent + Vec3f(cfg.searchSlack);
    searchBox.vmin = targetPoint - half;
    searchBox.vmax = targetPoint + half;

    bool spotFound = false;
    Vec3f bestPos;
    Vec3f bestRot;
    float bestScore = -1e30f;
    for (unsigned trial = 0; trial < cfg.pocketTrialCount; trial++) {
      unsigned rotIdx = trial % unsigned(scene.randAngles.size());
      Vec3f rot = scene.randAngles[rotIdx];
      const VoxFootprint &footprint =
          GetOrBuildFootprint(footprintCache, kindItemIndex, rotIdx, item.mesh, rot, scene.dx);

      SpotScore score;
      score.mode = SpotScoreMode::POCKET;
      const CoveragePatch &patch = field.patches[patchId];
      score.scorer = [&](const Vec3f &disp, const Vec3f & /*coord*/) {
        unsigned closed = CountRaysBlocked(field, patch.rayIdx, footprint, disp, scene.dx);
        return float(closed) * 1000.0f - (disp - targetPoint).norm();
      };

      Vec3f pos;
      if (!FindSpotLocal(scene.bg, footprint, searchBox, score, pos)) {
        continue;
      }
      float s = score.scorer(pos, pos);
      if (s > bestScore) {
        bestScore = s;
        bestPos = pos;
        bestRot = rot;
        spotFound = true;
      }
    }

    if (!spotFound) {
      planner.RecordFail(patchId);
      continue;
    }

    RigidTransform tran;
    tran.position = bestPos;
    tran.rotation = RotationMatrixRad(bestRot[0], bestRot[1], bestRot[2]);
    Vec3f pushDir = PocketPushDirection(field, rayIdx);
    std::vector<RigidTransform> trajectory;
    NudgeResult nudgeResult;
    RigidTransform settled = scene.Nudge(kindItemIndex, tran, pushDir, trajectory, &nudgeResult);

    // H3 accept/reject, computed from the settled transform alone (Nudge
    // does not mutate bg/classGrid, so a rejected placement costs nothing
    // to drop).

    // the container is not part of the final printed/milled object, so a
    // berry resting only on it is a loose piece in the physical part --
    // reject outright rather than accept-with-a-warning. One exception:
    // if the scene has no items in it at all yet, there is nothing to
    // contact by definition, and this would reject every placement
    // forever, unable to ever bootstrap. A real physical build has the
    // same constraint -- the first piece has to touch the mold/floor,
    // everything after nests against what is already there -- so this
    // allows exactly that first moment and nothing past it: the instant
    // any item exists, the strict rule above applies to every attempt
    // after, including on patches nowhere near this first placement.
    if (nudgeResult.itemContactCount == 0) {
      if (!scene.instances.empty()) {
        result.rejectedNoContact++;
        planner.RecordFail(patchId);
        continue;
      }
      result.bootstrapPlacements++;
    }

    Vec3f rayDir = field.rays[rayIdx].dir; // points inward, away from the mouth.
    Vec3f moved = settled.position - tran.position;
    float inwardSink = moved.dot(rayDir);
    float maxInwardSink = itemMaxExtent; // one berry diameter.
    if (inwardSink > maxInwardSink) {
      result.rejectedInwardSink++;
      planner.RecordFail(patchId);
      continue;
    }

    // lateral drift off the target ray's line, not distance from patch.center
    // (a surface point): the aim point now legitimately sits deep in the
    // pocket, up to ray.depth from the surface, so raw 3D distance from a
    // surface-anchored point would reject most successful deep placements
    // for the depth they were aimed at, not for actually wandering off
    // the patch they were assigned to.
    Vec3f relFromRay = settled.position - targetRay.origin;
    Vec3f lateral = relFromRay - relFromRay.dot(rayDir) * rayDir;
    float patchSlackRadius = 2.0f * cfg.coverage.patchSize;
    if (lateral.norm() > patchSlackRadius) {
      result.rejectedLeftPatch++;
      planner.RecordFail(patchId);
      continue;
    }

    // approximates the "still open" reject: did the settle end up roughly
    // within the ray's own open run, deep enough to actually plug it,
    // instead of drifting back out toward the mouth or past the far wall
    // it was backed off from? Bounding this to a tight window around one
    // exact aim point (the original version of this check) rejected most
    // legitimate deep placements too: Nudge is expected to slide the item
    // along the ray to find real contact, sometimes most of the way from
    // mouth to hit point, so the along-ray position needs the whole open
    // run as its allowed range, not a ~2 cm window. Lateral drift off the
    // ray line is the separate, tighter "left patch" check above; this one
    // is purely "still somewhere between the mouth and the far wall." A
    // precise re-march would need the settled footprint at the settled
    // rotation, which is not cached; this is the cheaper stand-in until
    // measured otherwise.
    float alongRay = relFromRay.dot(rayDir);
    float alongSlack = itemMaxExtent + cfg.searchSlack;
    if (alongRay < mouthDepth - alongSlack || alongRay > hitDepth + alongSlack) {
      result.rejectedStillOpen++;
      planner.RecordFail(patchId);
      continue;
    }

    unsigned instanceId = scene.Put(kindItemIndex, settled);
    scene.instances[instanceId].trajectory = trajectory;
    planner.RecordSuccess(patchId, kindItemIndex);
    result.placed++;

    for (unsigned pid : FindNeighborPatches(field, patchId, cfg.coverage.patchSize * 1.5f)) {
      ReprobePatch(field, pid, scene.classGrid, scene.WorldOrigin());
    }
  }

  result.field = std::move(field);
  return result;
}
