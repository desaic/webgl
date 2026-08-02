#include "BenchReport.h"
#include "BenchScene.h"
#include "Benchmark.h"
#include "MemStats.h"
#include "MeshOps.h"
#include "PackingDriver.h"
#include "PackingOps.h"
#include "Profiler.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <ostream>
#include <sstream>

namespace {

void GrowthHeader() {
  std::cout << "  " << std::left << std::setw(9) << "placed" << std::right
            << std::setw(12) << "wall_ms" << std::setw(12) << "rss"
            << std::setw(12) << "meshCache" << std::setw(12) << "trigGrids"
            << std::setw(8) << "entries" << std::setw(12) << "traj" << "\n";
}

void GrowthRow(unsigned placed, double wallMs, const SceneMemory &m) {
  std::cout << "  " << std::left << std::setw(9) << placed << std::right
            << std::fixed << std::setprecision(1) << std::setw(12) << wallMs
            << std::setw(12) << FormatBytes(RssBytes()) << std::setw(12)
            << FormatBytes(m.instanceMeshCache) << std::setw(12)
            << FormatBytes(m.instanceGrids) << std::setw(8)
            << m.instanceGridsCount << std::setw(12)
            << FormatBytes(m.trajectories) << "\n";
  std::cout.unsetf(std::ios::floatfield);
}

} // namespace

// places itemCount small items into a partly full container one at a time,
// printing memory after each. instanceMeshCache and instanceGrids are never
// cleared, so both columns should only ever climb.
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
