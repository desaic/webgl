#include "BenchReport.h"
#include "BenchScene.h"
#include "Benchmark.h"
#include "MeshOps.h"
#include "PackValidate.h"
#include "PackingOps.h"
#include "Profiler.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace {

// mirrors the shrink FindSpotSubgrid applies internally. duplicated here
// only to report it; the real one lives in PackingOps.cpp.
float ShrinkScale(float maxExtent) {
  if (maxExtent < 5.0f && maxExtent > 0.5f) {
    return (maxExtent - 0.5f) / maxExtent;
  }
  return 1.0f;
}

} // namespace

// the 6k-blueberry case. small items are the ones packed in bulk, so their
// per placement cost and their actual overlap behaviour decide whether the
// run is feasible at all. FindSpotSubgrid shrinks anything under 5 cm by a
// flat 0.5 cm before the FFT, which for a sub-centimeter berry removes most
// of its volume, so this scenario reports the shrink alongside validation.
void BenchSmallFruitBatch(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, true);
  if (!bs.ok) {
    return;
  }
  PackingScene &scene = *bs.scene;

  // smallest item in the set, i.e. the smallest blueberry.
  std::vector<unsigned> picks = PickSmallItems(bs, 1);
  if (picks.empty()) {
    return;
  }
  unsigned itemIdx = picks[0];
  MeshInfo &item = scene.items[itemIdx];
  Vec3f ext = item.box.vmax - item.box.vmin;
  float maxExtent = std::max({ext[0], ext[1], ext[2]});
  float scale = ShrinkScale(maxExtent);

  // validation always on here: the point is whether bulk small placements
  // are geometrically sound, not just how fast they are.
  PackValidator validator;
  validator.Init(scene, ctx.cfg);
  validator.AddAllPlaced(scene);

  unsigned totalCells = scene.numSubgridCells[0] * scene.numSubgridCells[1]
                        * scene.numSubgridCells[2];
  unsigned target = ctx.itemCount * 8;
  unsigned angleIndex = 0;
  unsigned placed = 0, failures = 0, attempts = 0;
  float worstOverlap = 0.0f, worstOutside = 0.0f;

  BenchRegion r("smallfruit_batch");
  std::ostringstream head;
  head << item.name << " extent " << maxExtent << " cm, shrunk to "
       << (maxExtent * scale) << " cm for the FFT (" << (100.0f * scale)
       << "% linear, " << (100.0f * scale * scale * scale) << "% volume, "
       << (maxExtent * scale / ctx.cfg.dx) << " voxels at dx " << ctx.cfg.dx
       << ")";
  r.Note(head.str());

  double deadlineMs = 1000.0 * double(std::min(ctx.RemainingSec(), 60.0f));
  for (unsigned n = 0; n < target && r.ElapsedMS() < deadlineMs; n++) {
    bool done = false;
    for (unsigned trial = 0; trial < ctx.cfg.maxTrialCount && !done; trial++) {
      Vec3f pos;
      Vec3f rot = scene.randAngles[angleIndex];
      angleIndex = (angleIndex + 1) % unsigned(scene.randAngles.size());
      attempts++;
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

      ValidationResult vr = validator.ValidatePlacement(scene, itemIdx, newTran);
      if (!vr.Ok()) {
        failures++;
      }
      worstOverlap = std::max(worstOverlap, vr.maxOverlapDepth);
      worstOutside = std::max(worstOutside, vr.maxOutsideDepth);
      validator.AddPlaced(scene, itemIdx, newTran);

      unsigned instanceId = scene.Put(itemIdx, newTran);
      scene.instances[instanceId].trajectory = trajectory;
      placed++;
      done = true;
    }
  }

  std::ostringstream oss;
  oss << placed << " placed in " << attempts << " search attempts";
  if (placed > 0) {
    oss << " (" << (double(attempts) / double(placed)) << " attempts and "
        << (r.ElapsedMS() / double(placed)) << " ms per placement)";
  }
  r.Note(oss.str());

  std::ostringstream v;
  v << failures << " of " << placed << " placements failed validation,"
    << " worst overlap " << worstOverlap << " cm, worst outside "
    << worstOutside << " cm";
  r.Note(v.str());

  if (placed > 0) {
    // a full run should finish in a few hours. with a few thousand small
    // placements in it, that leaves roughly a second each, so compare
    // against that rather than against an absolute number.
    double perMs = r.ElapsedMS() / double(placed);
    std::ostringstream e;
    e << perMs << " ms per placement vs a ~1000 ms budget implied by a"
      << " few-hour run with a few thousand placements";
    r.Note(e.str());
  }
  r.Finish(&scene);
}
