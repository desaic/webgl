#include "BenchReport.h"
#include "BenchScene.h"
#include "Benchmark.h"
#include "MemStats.h"
#include "MeshOps.h"
#include "PackValidate.h"
#include "PackingDriver.h"
#include "PackingOps.h"
#include "Profiler.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <ostream>
#include <sstream>

namespace {

void GrowthHeader() {
  std::cout << "  " << std::left << std::setw(9) << "placed" << std::right
            << std::setw(12) << "wall_ms" << std::setw(12) << "rss"
            << std::setw(12) << "kindGrids" << std::setw(8) << "entries"
            << std::setw(12) << "traj" << "\n";
}

void GrowthRow(unsigned placed, double wallMs, const SceneMemory &m) {
  std::cout << "  " << std::left << std::setw(9) << placed << std::right
            << std::fixed << std::setprecision(1) << std::setw(12) << wallMs
            << std::setw(12) << FormatBytes(RssBytes()) << std::setw(12)
            << FormatBytes(m.kindGrids) << std::setw(8)
            << m.kindGridsCount << std::setw(12)
            << FormatBytes(m.trajectories) << "\n";
  std::cout.unsetf(std::ios::floatfield);
}

} // namespace

// places itemCount small items into a partly full container one at a time,
// printing memory after each. kindGrids is keyed by item kind and never
// cleared, so it climbs only until each kind has been met once, then flattens.
// before the grids were keyed by kind this column grew per placement.
void BenchCacheGrowth(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, true);
  if (!bs.ok) {
    return;
  }
  PackingScene &scene = *bs.scene;
  std::vector<unsigned> picks = PickSmallItems(bs, ctx.itemCount);
  if (picks.empty()) {
    return;
  }

  BenchNote("baseline before any placement:");
  GrowthHeader();
  GrowthRow(0, 0.0, MeasureSceneMemory(scene));

  BenchRegion r("cache_growth");
  unsigned totalCells = scene.numSubgridCells[0] * scene.numSubgridCells[1]
                        * scene.numSubgridCells[2];
  unsigned angleIndex = 0;
  unsigned placed = 0;
  for (unsigned n = 0; n < ctx.itemCount; n++) {
    MeshInfo &item = scene.items[picks[n % picks.size()]];
    unsigned itemIdx = picks[n % picks.size()];
    double before = r.ElapsedMS();
    bool done = false;
    for (unsigned trial = 0; trial < ctx.cfg.maxTrialCount && !done; trial++) {
      Vec3f pos;
      Vec3f rot = scene.randAngles[angleIndex];
      angleIndex = (angleIndex + 1) % unsigned(scene.randAngles.size());
      if (!FindSpotSubgrid(scene.bg, item.mesh, pos, rot, scene.sdf, 1.0f,
                           scene.subgridCellSize,
                           (n * ctx.cfg.maxTrialCount + trial) % totalCells,
                           scene.numSubgridCells, false)) {
        continue;
      }
      RigidTransform tran;
      tran.position = pos;
      tran.rotation = RotationMatrixRad(rot[0], rot[1], rot[2]);
      Vec3f pushDir = scene.ForceDirection(itemIdx, Vec3f(-1, 0, 0), 1.0f, tran);
      std::vector<RigidTransform> trajectory;
      RigidTransform newTran = scene.Nudge(itemIdx, tran, pushDir, trajectory);
      unsigned instanceId = scene.Put(itemIdx, newTran);
      scene.instances[instanceId].trajectory = trajectory;
      placed++;
      done = true;
    }
    if (done) {
      GrowthRow(placed, r.ElapsedMS() - before, MeasureSceneMemory(scene));
    }
  }
  std::ostringstream oss;
  oss << placed << " placed";
  r.Note(oss.str());
  r.Finish(&scene);
}

// Put re-voxelizes one item and ORs it into bg, so its cost should be flat
// in instance count. this checks that, and that broadPhase.Add stays cheap.
void BenchPutScaling(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, true);
  if (!bs.ok) {
    return;
  }
  PackingScene &scene = *bs.scene;
  std::vector<unsigned> picks = PickSmallItems(bs, 1);
  if (picks.empty()) {
    return;
  }
  unsigned itemIdx = picks[0];

  // same item, same pose, repeated. isolates Put from FindSpot and Nudge.
  RigidTransform tran;
  tran.position = 0.5f * (scene.container.box.vmin + scene.container.box.vmax);
  tran.rotation = Matrix3f::identity();

  BenchRegion r("put_scaling");
  unsigned reps = ctx.itemCount * 8;
  double firstQuarter = 0.0, lastQuarter = 0.0;
  for (unsigned i = 0; i < reps; i++) {
    double t0 = r.ElapsedMS();
    scene.Put(itemIdx, tran);
    double dtMs = r.ElapsedMS() - t0;
    if (i < reps / 4) {
      firstQuarter += dtMs;
    } else if (i >= reps - reps / 4) {
      lastQuarter += dtMs;
    }
  }
  unsigned q = std::max(1u, reps / 4);
  std::ostringstream oss;
  oss << reps << " Puts, first quarter mean " << (firstQuarter / q)
      << " ms, last quarter mean " << (lastQuarter / q) << " ms";
  r.Note(oss.str());
  r.Finish(&scene);
}

// one full plan step with a small placement budget. the only scenario that
// exercises the real control flow, including noMoreFit and cell advance.
void BenchEndToEnd(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, true);
  if (!bs.ok) {
    return;
  }
  if (ctx.cfg.startStep >= ctx.plan.steps.size()) {
    std::cout << "  startStep " << ctx.cfg.startStep << " past plan size "
              << ctx.plan.steps.size() << ", skipping\n";
    return;
  }
  PackingStep step = ctx.plan.steps[ctx.cfg.startStep];
  step.count = ctx.itemCount;

  PackingConfig local = ctx.cfg;
  local.trajSaveInterval = 0;
  local.packSaveInterval = 0;
  // near a full container a single placement costs thousands of failed
  // searches, so this scenario is bounded by time, not by placement count.
  // it runs last and takes whatever budget is left, up to 90 s.
  float budget = ctx.RemainingSec();
  local.maxSecondsPerStep = budget < 90.0f ? budget : 90.0f;
  if (local.maxSecondsPerStep < 5.0f) {
    BenchSkip("end_to_end",
              "one plan step through the real PackStep control flow",
              "not enough time budget left");
    return;
  }

  size_t before = bs.scene->instances.size();
  BenchRegion r("end_to_end");
  PackStep(*bs.scene, step, local);
  size_t placed = bs.scene->instances.size() - before;
  std::ostringstream oss;
  oss << "step " << ctx.cfg.startStep << ", time cap "
      << local.maxSecondsPerStep << " s, " << placed << " newly placed onto "
      << before << " existing";
  r.Note(oss.str());

  // the search count is the point of this scenario. reporting only the
  // placement count would make a saturated container look like a no-op
  // when it actually burned the whole budget failing.
  // FindSpotSubgrid calls FindSpot internally, so the two counters
  // overlap; the subgrid calls are the outer attempts and the remainder
  // of findspot.total is the full-container path.
  unsigned long subgrid = Profiler::Calls("findspot_subgrid.total");
  unsigned long inner = Profiler::Calls("findspot.total");
  unsigned long fullGrid = inner > subgrid ? inner - subgrid : 0;
  unsigned long searches = subgrid + fullGrid;
  std::ostringstream eff;
  eff << searches << " spot searches (" << subgrid << " subgrid, "
      << fullGrid << " full container) for " << placed << " placements";
  if (placed > 0) {
    eff << " (" << (double(searches) / double(placed))
        << " searches and "
        << ((r.ElapsedMS() / double(placed)) / 1000.0)
        << " s per placement)";
  } else {
    eff << " -- container is saturated at this fill level, the whole"
           " budget went to failed searches";
  }
  r.Note(eff.str());
  r.Finish(bs.scene.get());
}

// the 6k-blueberry feasibility check, through the real PackStep.
//
// PackStep retires a subgrid cell for an item as soon as that item has been
// placed in it, so before this was fixed each kind could be placed at most
// once per cell. with 90 cells and ~10 small kinds the whole run capped out
// near 900 blueberries against a target of 6000+. the ceiling is reported
// next to the actual count so a regression shows up as a number, not a
// vague slowdown.
void BenchSmallFruitPackStep(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, true);
  if (!bs.ok) {
    return;
  }
  PackingScene &scene = *bs.scene;
  if (ctx.plan.steps.empty()) {
    return;
  }
  // the last plan step is the small group, i.e. the blueberries.
  PackingStep step = ctx.plan.steps.back();
  step.count = 1000000u;

  PackingConfig local = ctx.cfg;
  local.trajSaveInterval = 0;
  local.packSaveInterval = 0;
  local.startItem = 0;
  float budget = ctx.RemainingSec();
  local.maxSecondsPerStep = budget < 60.0f ? budget : 60.0f;
  if (local.maxSecondsPerStep < 5.0f) {
    BenchSkip("smallfruit_packstep",
              "bulk small placement through the real PackStep control flow",
              "not enough time budget left");
    return;
  }

  unsigned totalCells = scene.numSubgridCells[0] * scene.numSubgridCells[1]
                        * scene.numSubgridCells[2];
  size_t before = scene.instances.size();

  BenchRegion r("smallfruit_packstep");
  PackStep(scene, step, local);
  size_t placed = scene.instances.size() - before;

  // per kind counts, to compare against the one-per-cell ceiling.
  std::map<unsigned, unsigned> perKind;
  for (size_t i = before; i < scene.instances.size(); i++) {
    perKind[scene.instances[i].itemId]++;
  }
  unsigned worstKind = 0;
  for (auto &kv : perKind) {
    if (kv.second > worstKind) {
      worstKind = kv.second;
    }
  }

  std::ostringstream oss;
  oss << placed << " placed onto " << before << " existing in "
      << local.maxSecondsPerStep << " s, " << step.names.size()
      << " kinds in the step, " << perKind.size() << " kinds used";
  r.Note(oss.str());

  // cursor sum is the direct evidence. cells are only retired on a cell
  // that failed every trial, so with the fix in place the cursors advance
  // far less than the placement count. if a placement still retired its
  // cell, cursorSum would be >= placed.
  unsigned cursorSum = 0;
  for (unsigned i = 0; i < step.names.size(); i++) {
    cursorSum += scene.items[scene.GetItemIndex(step.names[i])].nextCellIdx;
  }

  std::ostringstream cap;
  cap << totalCells << " subgrid cells per kind, ceiling "
      << (totalCells * step.names.size()) << " placements if cells retire on"
      << " success; most placements of one kind " << worstKind;
  r.Note(cap.str());

  std::ostringstream cur;
  cur << "cell cursors advanced " << cursorSum << " total for " << placed
      << " placements";
  if (placed > 0 && cursorSum < placed) {
    cur << " -- cells are being reused, cap is lifted";
  } else {
    cur << " -- cells still retire per placement, cap is NOT lifted";
  }
  r.Note(cur.str());

  if (placed > 0) {
    std::ostringstream rate;
    rate << (r.ElapsedMS() / double(placed)) << " ms per placement, "
         << (double(Profiler::Calls("findspot_subgrid.total")) / double(placed))
         << " subgrid searches per placement";
    r.Note(rate.str());
  }

  // any speedup in the narrow phase has to be paid for in overlap, so the
  // whole batch is re-checked here rather than trusting the 3-placement
  // nudge scenarios. each new berry is validated against the container and
  // against everything placed before it, in placement order, so this is the
  // same test smallfruit_batch runs inline.
  if (ctx.validate && placed > 0) {
    PackValidator validator;
    validator.Init(scene, ctx.cfg);
    for (size_t i = 0; i < before; i++) {
      validator.AddPlaced(scene, scene.instances[i].itemId,
                          scene.instances[i].tran);
    }
    unsigned failures = 0, deepish = 0;
    float worstOverlap = 0.0f, worstOutside = 0.0f, overlapSum = 0.0f;
    for (size_t i = before; i < scene.instances.size(); i++) {
      const InstanceInfo &inst = scene.instances[i];
      ValidationResult vr =
          validator.ValidatePlacement(scene, inst.itemId, inst.tran);
      if (!vr.Ok()) {
        failures++;
      }
      if (vr.maxOverlapDepth > 0.5f * validator.allowedDepth) {
        deepish++;
      }
      overlapSum += vr.maxOverlapDepth;
      worstOverlap = std::max(worstOverlap, vr.maxOverlapDepth);
      worstOutside = std::max(worstOutside, vr.maxOutsideDepth);
      validator.AddPlaced(scene, inst.itemId, inst.tran);
    }
    std::ostringstream val;
    val << "validated " << placed << " new placements, " << failures
        << " failures, worst overlap " << worstOverlap << " cm, worst outside "
        << worstOutside << " cm, mean per-placement overlap "
        << (overlapSum / double(placed)) << " cm, " << deepish << " deeper than "
        << (0.5f * validator.allowedDepth) << " cm";
    r.Note(val.str());

    // written so the pack can actually be looked at. a number saying the
    // overlap is small does not show whether the berries ended up spread
    // over the surface or piled in one corner.
    std::string dump = "smallfruit_packstep_pack.txt";
    scene.SaveInstances(dump);
    r.Note("pack written to " + dump + " for visual inspection");
  }
  r.Finish(&scene);
}
