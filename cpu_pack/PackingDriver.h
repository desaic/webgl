#pragma once

#include "PackingConfig.h"
#include "PackingPlan.h"
#include "PackingScene.h"
#include "PackShrinkWrap.h"

#include <utility>
#include <vector>

/// loads items and container meshes, inits broad phase, container sdf
/// and acceleration grids. does not voxelize the packing background.
/// benchmarks call this to get a ready scene without packing anything.
/// @return false if the item directory or container mesh is missing.
bool BuildScene(PackingScene &scene, const PackingConfig &cfg);

/// voxelizes the container into the packing occupancy grid and inverts it
/// so that the exterior is unavailable.
void PrepareBackground(PackingScene &scene, const PackingConfig &cfg);

/// per-item-kind settle time accumulator, threaded (as an optional pointer)
/// through PackStep/TryFillSpot/PackFillVoids/PackFillCrevices so a single
/// PackScene run builds up "what a settle costs, by kind" instead of that
/// information only ever appearing as scattered per-placement LOGI lines.
/// Every NudgeToTarget/Nudge call, not just successful ones, is recorded --
/// a failed settle still pays the full step-loop cost, and in Phase 2 most
/// of them fail (see plan.txt CURRENT STATUS), so leaving those out would
/// undercount round time badly.
struct PlacementTimer {
  struct Stats {
    unsigned count = 0;
    double totalMs = 0.0;
    double minMs = 1e18;
    double maxMs = 0.0;
  };
  std::map<std::string, Stats> byName;
  void Record(const std::string &name, double ms);
};

/// writes one line per kind ("name: count=N total_ms=T avg_ms=A min=.. max=.."),
/// sorted by total_ms descending so the dominant cost is first.
void SavePlacementTimer(const PlacementTimer &timer, const std::string &filename);

/// runs one step of the plan: repeatedly find a spot and nudge an item in.
/// when surfacePoints is non-empty, small-fruit nudges target the centroid of
/// the 5 nearest free container-surface points instead of the default force field.
/// timer, if non-null, records every settle (see PlacementTimer above).
void PackStep(PackingScene &scene, const PackingStep &step, const PackingConfig &cfg,
              const std::vector<Vec3f> &surfacePoints = {}, PlacementTimer *timer = nullptr);

/// prepares the background, optionally resumes from a pack file,
/// then runs plan steps starting at cfg.startStep.
void PackScene(PackingScene &scene, const PackingPlan &plan, const PackingConfig &cfg);

/// end to end: build the scene then pack it.
void PackFruits(const PackingPlan &plan, const PackingConfig &cfg);

/// casts one inward ray per container surface sample point against all
/// placed instances, saves surface_depths.obj and deep_rays.obj under
/// scene.outputFolder, and returns the deep ray origins/ends. read-only:
/// does not place, settle, or otherwise mutate scene, so it is safe to
/// call without running any packing steps.
void ComputeSurfaceDepths(PackingScene &scene,
                         std::vector<Vec3f> &deepOrigins,
                         std::vector<Vec3f> &deepEnds);

/// debug helper: finds the container-surface ray closest to targetPos and
/// prints its depth plus every neighbor ray within the same radius
/// FindDeepRays uses, so a spurious "deep ray" flag on a curved surface
/// can be diagnosed without running the packing steps.
void DebugDeepRayNeighbors(PackingScene &scene, const Vec3f &targetPos);

/// marching-cubes a 2-voxel inset of the container SDF, samples the resulting
/// mesh, and returns only the points whose voxel is unoccupied in scene.bg.vox.
std::vector<Vec3f> ComputeFreeContainerPoints(PackingScene &scene);

/// calls ComputeFreeContainerPoints and writes the result to filename.
void SaveFreeContainerSurface(PackingScene &scene, const std::string &filename);

/// sparse "claimed this round" tracker: a fruit was just placed here, so
/// skip attempting a nearby candidate spot that turns out to be the same
/// physical gap. Claim radius comes from the ACTUALLY PLACED item's own
/// size at its ACTUALLY SETTLED position (see TryFillSpot), not a guess
/// made at spot-generation time -- a flat radius mismatches between fruit
/// tiers. See plan.txt SHARED HELPERS #2 / CANDIDATE SPOT MANAGEMENT.
/// Deliberately a plain O(n) list, not a real spatial grid: round sizes
/// (tens to low hundreds of claims) do not need one yet.
class ClaimGrid {
  public:
    bool IsClaimed(const Vec3f &pos) const;
    void Mark(const Vec3f &pos, float radius);

  private:
    std::vector<std::pair<Vec3f, float>> claims;
};

/// tries each item kind in kindsBySize (expected sorted by BoxDiagonal
/// descending) at target. sizeMustFitLocalWidth (default true, PackFillVoids'
/// setting -- the old nominal-size fill pass, removed from the real
/// pipeline but still used by DebugFillVoids/benchmarks) skips a kind whose
/// BoxDiagonal()+clearance does not fit within localWidth -- PackFillVoids'
/// voids are enclosed gaps BETWEEN fruit, so a kind has to physically nest
/// inside one. Phase 2 (PackFillCrevices) wall crevices are a
/// different physical situation (see plan.txt CORE IDEA, revised again
/// after the user's manufacturability note): a small fruit resting ON TOP
/// of a crevice, bridging its mouth, is exactly the point -- it does not
/// need to fit inside the gap, it needs to shrink the effective open
/// cavity down to something small enough to ignore at manufacturing time.
/// PackFillCrevices therefore passes sizeMustFitLocalWidth=false. This is
/// safe now in a way the identically-named experiment earlier in this file
/// (see plan.txt TRIED AND REVERTED) was not: kindsBySize there was the
/// WHOLE catalogue including container-sized meshes; PackFillCrevices
/// scopes kindsBySize to the small-fruit categories up front, and the
/// container-containment check below (added for that revert) independently
/// rejects any pose sticking out through the wall regardless.
/// Settles with NudgeToTarget, and Puts on the first kind whose post-settle
/// overlap against envelope is below kOccupiedThresh (0.5, same threshold
/// N3 already uses) AND whose post-settle samples land inside the
/// container (scene.sdf <= 0) -- the container check exists because nothing
/// else here verifies a settled item is even inside the container at all;
/// normally the localWidth fit check makes that moot, but it is a real,
/// cheap, independent gap otherwise. Falls through to the next smaller kind
/// on a failed settle instead of giving up on the spot. Marks claims on
/// success. Returns the new instance id, or -1 if nothing in kindsBySize
/// fits or settles cleanly here. See plan.txt SPOT-PICKS-FRUIT and SHARED
/// HELPERS #3.
/// envelope is an EnvelopeField (PackShrinkWrap.h) built from the SAME
/// placed instances at spot-extraction time -- deliberately NOT
/// OccupiedFraction/scene.bg.vox, which is a much coarser grid (cfg.dx,
/// often 3x+ this field's voxel size) built by crude per-instance
/// voxelization; comparing a fine spot-finding result against that coarse,
/// dilation-biased grid made every settle read as fully occupied even when
/// it settled cleanly into real open space.
/// timer, if non-null, records every settle (see PlacementTimer above).
int TryFillSpot(PackingScene &scene, const Vec3f &target, float localWidth,
                const std::vector<unsigned> &kindsBySize, float clearance,
                const EnvelopeField &envelope, ClaimGrid &claims,
                PlacementTimer *timer = nullptr, bool sizeMustFitLocalWidth = true);

/// The old nominal-size fill pass (plan.txt PIPELINE, pre-rename Phase 2),
/// iterated: each round recomputes VoidField + spots + EnvelopeField from
/// the CURRENT instances (so a spot filled last round, or a gap it left
/// behind, is seen fresh), then runs ClaimGrid + TryFillSpot widest-spot-
/// first over the new spot list. Stops when a round places fewer than
/// cfg.fillVoidMinPlaced instances or cfg.fillVoidMaxRounds is hit. Nominal
/// size only (see plan.txt MULTI-SCALE FILL) -- kindsBySize is every item
/// currently in the scene. REMOVED from the real pipeline per the user
/// (plan.txt CURRENT STATUS) -- kept only for DebugFillVoids/benchmarks.
void PackFillVoids(PackingScene &scene, const PackingConfig &cfg, PlacementTimer *timer = nullptr);

/// the crevices Phase 2 is allowed to target, computed ONCE from the pack as
/// it stood after Phase 1 and never regenerated -- so small fruit placed by
/// Phase 2 cannot create new targets for later small fruit (plan.txt
/// PIPELINE Phase 2, FIXED CREVICE SET). Standalone grid data, no pointer
/// back into the scene.
struct CreviceBaseline {
  Vec3f origin = {0.0f, 0.0f, 0.0f};
  float voxelSize = 0.1f;
  // cleaned void mask at baseline, every width (1 = void). Used to tell
  // "original crevice space" from space that only became void later.
  Array3D8u allMask;
  // in-range (cfg.creviceMinWidth..creviceMaxWidth) spots, widest first.
  std::vector<VoidSpot> spots;
  // per spot: world centers of the baseline mask voxels assigned to it
  // (nearest spot by distance-to-inscribed-sphere). Together these are C0.
  std::vector<std::vector<Vec3f>> spotVoxels;
  // instances [0, numBaseInstances) existed when this was computed.
  size_t numBaseInstances = 0;
};

/// builds the baseline from scene.instances[0, numInstances) only.
CreviceBaseline ComputeCreviceBaseline(PackingScene &scene, const PackingConfig &cfg,
                                       size_t numInstances);

/// Phase 2: small fruit resting on top of fruit-fruit crevices, targets
/// restricted to baseline.spots. Each round recomputes the current void
/// field only to decide whether a baseline spot is already closed (less
/// than cfg.creviceClosedFrac of its baseline voxels still open) -- never
/// to generate new spots. Open spots get TryFillSpot with the small kinds
/// (sizeMustFitLocalWidth=false: rest ON TOP of the crevice), at most
/// cfg.creviceMaxAttemptsPerSpot times per spot. Stops when a round places
/// fewer than cfg.creviceMinPlaced or cfg.creviceMaxRounds is hit.
void PackFillCrevices(PackingScene &scene, const PackingConfig &cfg,
                      const std::vector<std::string> &smallItemNames,
                      const CreviceBaseline &baseline,
                      PlacementTimer *timer = nullptr);

/// how much of the baseline crevice set C0 Phase 2 actually closed, and
/// which Phase 2 fruit did the closing. A fruit that closed less than
/// cfg.creviceUsefulVolume of C0 is unnecessary (e.g. resting on a smooth
/// fruit surface with no original crevice under it).
struct CreviceCoverageReport {
  float c0Volume = 0.0f;           // cm^3, all baseline in-range crevice voxels
  float stillOpenVolume = 0.0f;    // cm^3 of C0 still open now
  float closedVolume = 0.0f;       // c0Volume - stillOpenVolume
  float unattributedClosed = 0.0f; // closed C0 with no Phase 2 fruit within 1cm
  float newVoidVolume = 0.0f;      // cm^3 of current void not in the baseline mask
  unsigned spotsTotal = 0;
  unsigned spotsClosed = 0;
  struct Fruit {
    unsigned instanceId = 0;
    std::string itemName;
    Vec3f pos;  // pack-file frame, same as pack_final.txt
    float closedVolume = 0.0f;  // C0 cm^3 whose nearest Phase 2 fruit surface is this one
  };
  // every Phase 2 fruit, ascending by closedVolume (least useful first).
  std::vector<Fruit> fruits;
  unsigned unnecessaryCount = 0;
  float usefulVolume = 0.0f;  // the threshold used, for the report header
};

CreviceCoverageReport ComputeCreviceCoverage(PackingScene &scene, const PackingConfig &cfg,
                                             const CreviceBaseline &baseline);

/// writes the report to filename, plus the unnecessary fruit centers as a
/// point cloud to objFilename (skipped if empty).
void SaveCreviceCoverageReport(const CreviceCoverageReport &report, const std::string &filename,
                               const std::string &objFilename);

/// end-of-run quality numbers, meant to be re-run after any packing change
/// so an "improvement" claim has a number behind it instead of eyeballing
/// the traj obj (see plan.txt Phase 4). worstOffenders doubles as a
/// debugging tool: it says specifically which existing placement's
/// neighborhood the leftover open space sits next to, not just one global
/// percentage, so a specific bad placement choice can be pinned down.
struct PackQualityReport {
  float containerVolume = 0.0f;  // cm^3, straight from scene.sdf's own dense grid.
  float fruitVolume = 0.0f;      // cm^3, exact per-mesh volume (divergence theorem) x instance
                                 // count -- pose-independent, so this is not a voxel estimate.
  float openVolume = 0.0f;       // cm^3 of surviving "worth filling" space -- the SAME cleaned
                                 // mask ExtractVoidSpots/TryFillSpot already act on, at Phase
                                 // 2's own cfg.shrinkwrapVoxelSize (Phase 2 reuses the identical
                                 // VoidField, see PackFillCrevices), so this is fruit-fruit
                                 // crevice space the pipeline itself still considers fillable,
                                 // not raw unoccupied space and not anything wall-relative.
  float fillFraction = 0.0f;     // fruitVolume / containerVolume
  float openFraction = 0.0f;     // openVolume / containerVolume

  struct Offender {
    unsigned instanceId = 0;
    std::string itemName;
    Vec3f pos;
    float nearbyOpenVolume = 0.0f;  // cm^3 of open voxels whose nearest instance
                                    // CENTER (not surface) is this one -- an
                                    // approximation of "whose neighborhood this
                                    // gap belongs to", not exact attribution.
  };
  // top instances by nearbyOpenVolume, largest first: a placement that left
  // a lot of open space parked right next to it is worth checking (pose,
  // kind choice, settle outcome) specifically, rather than reading the two
  // fractions above and guessing where to look.
  std::vector<Offender> worstOffenders;
};

/// recomputes VoidField at cfg.shrinkwrapVoxelSize -- same field Phase 2
/// itself uses now -- so the metric matches what the pipeline is actually
/// trying to close. Safe to call at any point after PrepareBackground, not
/// just at the very end, so before/after numbers for a single change can
/// come from the same process run.
PackQualityReport ComputePackQuality(PackingScene &scene, const PackingConfig &cfg,
                                     unsigned numOffendersToReport = 10);

/// writes the report as "key: value" lines, one offender per line, to filename.
void SavePackQualityReport(const PackQualityReport &report, const std::string &filename);
