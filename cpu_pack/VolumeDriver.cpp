#include "VolumeDriver.h"

#include "MeshInfo.h"
#include "MeshOps.h"
#include "PackingOps.h"
#include "PackingPlan.h"
#include "PackingScene.h"
#include "PocketPlanner.h" // PocketKindInfo: same {itemIndex, maxExtent} shape H3 already uses.
#include "Profiler.h"
#include "Stopwatch.h"
#include "VoxFootprint.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace {

// every kind index, ranked largest-fits-first for pocketDiameter (in turn
// tie-broken by least used so far, share-capped kinds pushed to the back
// rather than dropped), followed by every kind that does not fit the
// estimate at all, largest first, as a last resort. `pocketDiameter` is
// only a heuristic (2x one BFS-approximate distance value, not an exact
// clearance in every direction, per H7), so a kind it says "does not fit"
// may still turn out to fit once FindSpotLocal actually tries it -- ranking
// it low rather than excluding it means the caller still gets a shot at it
// if every better-ranked kind fails at this exact spot.
//
// Earlier versions of this picked exactly one kind per spot: the largest
// that fit the estimate. When that one specific kind didn't physically fit
// (measured on the melon dev container: every `!spotFound` fail was the
// single largest kind, at a spot with plenty of room for a smaller one),
// the whole spot was abandoned instead of falling back -- see
// heuristic_plan.md H7's cascade-fix note.
std::vector<unsigned> RankKindsForDepth(const std::vector<PocketKindInfo> &kinds,
                                        float pocketDiameter,
                                        const std::unordered_map<unsigned, unsigned> &useCount,
                                        unsigned totalPlaced, float kindShareCap) {
  auto useOf = [&](unsigned itemIndex) {
    auto it = useCount.find(itemIndex);
    return it != useCount.end() ? it->second : 0u;
  };
  std::vector<unsigned> fits, tooBig;
  for (unsigned i = 0; i < kinds.size(); i++) {
    (kinds[i].maxExtent <= pocketDiameter ? fits : tooBig).push_back(i);
  }
  auto byExtentDesc = [&](unsigned a, unsigned b) { return kinds[a].maxExtent > kinds[b].maxExtent; };
  std::sort(tooBig.begin(), tooBig.end(), byExtentDesc);

  bool capActive = totalPlaced > 4;
  std::vector<unsigned> underCap, overCap;
  for (unsigned i : fits) {
    bool over = capActive
               && float(useOf(kinds[i].itemIndex)) > kindShareCap * float(totalPlaced + 1);
    (over ? overCap : underCap).push_back(i);
  }
  auto byExtentThenUse = [&](unsigned a, unsigned b) {
    if (kinds[a].maxExtent != kinds[b].maxExtent) {
      return kinds[a].maxExtent > kinds[b].maxExtent;
    }
    return useOf(kinds[a].itemIndex) < useOf(kinds[b].itemIndex);
  };
  std::sort(underCap.begin(), underCap.end(), byExtentThenUse);
  std::sort(overCap.begin(), overCap.end(), byExtentThenUse);

  std::vector<unsigned> ranked;
  ranked.reserve(kinds.size());
  ranked.insert(ranked.end(), underCap.begin(), underCap.end());
  ranked.insert(ranked.end(), overCap.begin(), overCap.end());
  ranked.insert(ranked.end(), tooBig.begin(), tooBig.end());
  return ranked;
}

// same idea as PocketDriver.cpp's PocketPushDirection -- push toward the
// nearest real material (an item, never the container), since the
// container is not part of the finished physical part. A volumetric pocket
// has no ray to inspect neighbors along, so this scans a small local box of
// classGrid around the target point directly instead.
Vec3f VolumePushDirection(const Array3D8u &classGrid, const Vec3f &gridOrigin, float dx,
                          const Vec3f &targetPoint, float radius) {
  Vec3u size = classGrid.GetSize();
  Vec3f relMin = targetPoint - Vec3f(radius) - gridOrigin;
  Vec3f relMax = targetPoint + Vec3f(radius) - gridOrigin;
  int x0 = std::max(0, int(std::floor(relMin[0] / dx)));
  int y0 = std::max(0, int(std::floor(relMin[1] / dx)));
  int z0 = std::max(0, int(std::floor(relMin[2] / dx)));
  int x1 = std::min(int(size[0]) - 1, int(std::ceil(relMax[0] / dx)));
  int y1 = std::min(int(size[1]) - 1, int(std::ceil(relMax[1] / dx)));
  int z1 = std::min(int(size[2]) - 1, int(std::ceil(relMax[2] / dx)));

  float bestD2 = 1e30f;
  Vec3f best;
  bool found = false;
  for (int z = z0; z <= z1; z++) {
    for (int y = y0; y <= y1; y++) {
      for (int x = x0; x <= x1; x++) {
        uint8_t c = classGrid(unsigned(x), unsigned(y), unsigned(z));
        if (c != VOX_ITEM_BIG && c != VOX_ITEM_SMALL) {
          continue;
        }
        Vec3f p = gridOrigin
                  + dx * (Vec3f(float(x), float(y), float(z)) + Vec3f(0.5f));
        float d2 = (p - targetPoint).norm2();
        if (d2 < bestD2) {
          bestD2 = d2;
          best = p;
          found = true;
        }
      }
    }
  }
  if (!found) {
    return Vec3f(0.0f);
  }
  Vec3f dir = best - targetPoint;
  if (dir.norm() < 1e-6f) {
    return Vec3f(0.0f);
  }
  dir.normalize();
  return dir;
}

} // namespace

VolumeStepResult PackVolumeStep(PackingScene &scene, const PackingStep &step,
                                const VolumeStepConfig &cfg) {
  PROFILE_SCOPE("volume.total");
  VolumeStepResult result;

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

  PocketVolumeField field =
      BuildPocketVolumeField(scene.classGrid, scene.WorldOrigin(), cfg.volume);
  if (kinds.empty()) {
    result.exitReason = "no small kinds in step";
    result.field = std::move(field);
    return result;
  }

  // every peak of every component, flattened and ranked deepest-first: one
  // giant blob's peaks interleave with a dozen small pockets' peaks by
  // depth alone, so the deepest pocket anywhere is always tried next,
  // exactly like ranking components used to, but now a single bad spot
  // costs one entry in this list instead of every remaining peak the
  // component it belongs to.
  struct Candidate {
    unsigned compId;
    unsigned peakIdx;
  };
  std::vector<Candidate> candidates;
  auto rebuildCandidates = [&]() {
    candidates.clear();
    for (unsigned c = 0; c < field.components.size(); c++) {
      for (unsigned p = 0; p < field.components[c].peaks.size(); p++) {
        candidates.push_back(Candidate{c, p});
      }
    }
    std::sort(candidates.begin(), candidates.end(), [&](const Candidate &a, const Candidate &b) {
      return field.components[a.compId].peaks[a.peakIdx].dist
             > field.components[b.compId].peaks[b.peakIdx].dist;
    });
  };
  rebuildCandidates();
  unsigned candidateIdx = 0;

  VoxFootprintCache footprintCache;
  std::unordered_map<unsigned, unsigned> kindUseCount;

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
    // the real "done" signal: every peak in the current field generation
    // (every one at or above minTargetDepth -- BuildPocketVolumeField
    // already filters at collection time) has been tried once and failed.
    if (candidateIdx >= candidates.size()) {
      result.exitReason = "no free pocket left at or above minTargetDepth";
      break;
    }

    const Candidate &cand = candidates[candidateIdx];
    const FreeComponent &comp = field.components[cand.compId];
    const FreeComponent::Peak &peak = comp.peaks[cand.peakIdx];
    Vec3f targetPoint = peak.point;
    int peakDist = peak.dist;

    float pocketDiameter = 2.0f * float(peakDist) * cfg.volume.dx;
    std::vector<unsigned> rankedKinds =
        RankKindsForDepth(kinds, pocketDiameter, kindUseCount, result.placed, cfg.kindShareCap);
    result.attempts++;

    // cascade down the ranked list at this exact spot instead of giving up
    // the whole spot when only the top-ranked kind doesn't physically fit
    // -- see RankKindsForDepth's comment for why this exists.
    bool spotFound = false;
    unsigned kindItemIndex = 0;
    Vec3f bestPos;
    Vec3f bestRot;
    for (unsigned rankIdx : rankedKinds) {
      unsigned candidateKindItemIndex = kinds[rankIdx].itemIndex;
      MeshInfo &candidateItem = scene.items[candidateKindItemIndex];

      float bestScore = -1e30f;
      Vec3f itemExtent = candidateItem.box.vmax - candidateItem.box.vmin;
      Box3f searchBox;
      Vec3f half = 0.5f * itemExtent + Vec3f(cfg.searchSlack);
      searchBox.vmin = targetPoint - half;
      searchBox.vmax = targetPoint + half;

      for (unsigned trial = 0; trial < cfg.pocketTrialCount; trial++) {
        unsigned rotIdx = trial % unsigned(scene.randAngles.size());
        Vec3f rot = scene.randAngles[rotIdx];
        const VoxFootprint &footprint = GetOrBuildFootprint(
            footprintCache, candidateKindItemIndex, rotIdx, candidateItem.mesh, rot, scene.dx);

        SpotScore score;
        score.mode = SpotScoreMode::POCKET;
        // the component's own local max is already the deepest point of
        // this pocket -- unlike the ray path there is no "rays closed"
        // count to maximize, so the tie-break (closest to that point) is
        // the whole score.
        score.scorer = [&](const Vec3f &disp, const Vec3f & /*coord*/) {
          return -(disp - targetPoint).norm();
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
      if (spotFound) {
        kindItemIndex = candidateKindItemIndex;
        break;
      }
    }

    if (!spotFound) {
      candidateIdx++;
      continue;
    }

    MeshInfo &item = scene.items[kindItemIndex];
    Vec3f itemExtent = item.box.vmax - item.box.vmin;
    float itemMaxExtent = std::max({itemExtent[0], itemExtent[1], itemExtent[2]});

    RigidTransform tran;
    tran.position = bestPos;
    tran.rotation = RotationMatrixRad(bestRot[0], bestRot[1], bestRot[2]);
    float pushRadius = std::max(float(peakDist) * cfg.volume.dx, itemMaxExtent) + cfg.searchSlack;
    Vec3f pushDir =
        VolumePushDirection(scene.classGrid, scene.WorldOrigin(), scene.dx, targetPoint, pushRadius);
    std::vector<RigidTransform> trajectory;
    NudgeResult nudgeResult;
    RigidTransform settled = scene.Nudge(kindItemIndex, tran, pushDir, trajectory, &nudgeResult);

    // same bootstrap exception as PocketDriver.cpp: a real reject the
    // instant any item exists, an allowed one-time exception before that.
    if (nudgeResult.itemContactCount == 0) {
      if (!scene.instances.empty()) {
        result.rejectedNoContact++;
        candidateIdx++;
        continue;
      }
      result.bootstrapPlacements++;
    }

    // no ray line here to decompose drift into lateral/along-ray parts, so
    // this is the one isotropic stand-in: did the settle stay within
    // roughly this pocket's own radius of the point it was aimed at, not
    // wander into a neighboring pocket or back out through a wall.
    float slack = std::max(float(peakDist) * cfg.volume.dx, itemMaxExtent) + itemMaxExtent
                 + cfg.searchSlack;
    if ((settled.position - targetPoint).norm() > slack) {
      result.rejectedTooFar++;
      candidateIdx++;
      continue;
    }

    unsigned instanceId = scene.Put(kindItemIndex, settled);
    scene.instances[instanceId].trajectory = trajectory;
    kindUseCount[kindItemIndex]++;
    result.placed++;

    // full rebuild after every success: the placement just consumed voxels
    // out of comp (and possibly split it), and nothing cheaper than a fresh
    // flood-fill/BFS pair is implemented yet -- cost not yet measured
    // (heuristic_plan.md H7), simplest correct thing until proven too slow.
    field = BuildPocketVolumeField(scene.classGrid, scene.WorldOrigin(), cfg.volume);
    rebuildCandidates();
    candidateIdx = 0;
  }

  result.field = std::move(field);
  return result;
}
