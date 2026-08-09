#include "BenchReport.h"
#include "BenchScene.h"
#include "Benchmark.h"
#include "MeshOps.h"
#include "PackingOps.h"
#include "Profiler.h"

#include <iostream>
#include <sstream>

namespace {

// runs FindSpot maxTrialCount times per item, matching what PackStep does
// for an item that never finds a spot. the bg FFT is redone every trial.
void FindSpotSweep(BenchScene &bs, const PackingConfig &cfg,
                   const std::vector<unsigned> &picks, bool subgrid,
                   BenchRegion &r) {
  PackingScene &scene = *bs.scene;
  unsigned totalCells = scene.numSubgridCells[0] * scene.numSubgridCells[1]
                        * scene.numSubgridCells[2];
  unsigned found = 0, tried = 0;
  unsigned angleIndex = 0;
  for (size_t i = 0; i < picks.size(); i++) {
    MeshInfo &item = scene.items[picks[i]];
    for (unsigned trial = 0; trial < cfg.maxTrialCount; trial++) {
      Vec3f pos;
      Vec3f rot = scene.randAngles[angleIndex];
      angleIndex = (angleIndex + 1) % unsigned(scene.randAngles.size());
      tried++;
      bool ok;
      if (subgrid) {
        ok = FindSpotSubgrid(scene.bg, item.mesh, pos, rot, scene.sdf, 1.0f,
                             scene.subgridCellSize, trial % totalCells,
                             scene.numSubgridCells, false);
      } else {
        ok = FindSpot(scene.bg, item.mesh, pos, rot, scene.sdf, 1.0f);
      }
      if (ok) {
        found++;
        break;
      }
    }
  }
  std::ostringstream oss;
  oss << picks.size() << " items, " << tried << " FindSpot calls, " << found
      << " found";
  r.Note(oss.str());
}

} // namespace

// big items go through the full container FFT. this is the dominant cost
// early in a run, and findspot.bg_fft should show it is redone per trial.
void BenchFindSpotBig(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, false);
  if (!bs.ok) {
    return;
  }
  std::vector<unsigned> picks = PickBigItems(bs, ctx.itemCount);
  BenchRegion r("findspot_big");
  FindSpotSweep(bs, ctx.cfg, picks, false, r);
  r.Finish(bs.scene.get());
}

// small items take the subgrid path, which crops bg to one cell first so
// the FFT is much smaller. compare findspot.bg_fft against findspot_big.
void BenchFindSpotSmall(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, false);
  if (!bs.ok) {
    return;
  }
  std::vector<unsigned> picks = PickSmallItems(bs, ctx.itemCount);
  BenchRegion r("findspot_small");
  FindSpotSweep(bs, ctx.cfg, picks, true, r);
  r.Finish(bs.scene.get());
}

// same searches against a partly full container. the collision grid has
// far fewer free cells, so score_scan does less work but rejects more.
void BenchFindSpotPartial(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, true);
  if (!bs.ok) {
    return;
  }
  std::vector<unsigned> picks = PickSmallItems(bs, ctx.itemCount);
  BenchRegion r("findspot_partial");
  std::ostringstream oss;
  oss << bs.scene->instances.size() << " items already placed";
  r.Note(oss.str());
  FindSpotSweep(bs, ctx.cfg, picks, true, r);
  r.Finish(bs.scene.get());
}
