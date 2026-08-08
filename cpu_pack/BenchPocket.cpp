#include "BenchReport.h"
#include "BenchScene.h"
#include "Benchmark.h"
#include "GridUtils.h"
#include "PackingPlan.h"
#include "PackValidate.h"
#include "PocketDriver.h"
#include "SurfaceCoverage.h"
#include "VoxClass.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace {

// visible berry fraction: the class grid has no instance identity by
// design (VoxClass.h), so attribution comes from the placed instances
// directly, matched to a ray's hit point by proximity.
BerryVisibilityInput CollectSmallInstances(const PackingScene &scene) {
  BerryVisibilityInput vis;
  vis.matchRadius = 1.0f;
  for (const InstanceInfo &inst : scene.instances) {
    const MeshInfo &item = scene.items[inst.itemId];
    Vec3f ext = item.box.vmax - item.box.vmin;
    float maxExtent = std::max({ext[0], ext[1], ext[2]});
    if (VoxClassOfExtent(maxExtent, VOX_SMALL_THRESH_CM) == VOX_ITEM_SMALL) {
      vis.smallInstanceCenters.push_back(inst.tran.position);
    }
  }
  return vis;
}

} // namespace

// H1: measures a loaded pack against the surface coverage metric with no
// placement policy involved. This is the yardstick heuristic_plan.md's H2
// onward is judged against -- run this unchanged before and after any
// targeting change and compare open ray fraction / visible berry fraction.
void BenchCoverageBaseline(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, true);
  if (!bs.ok) {
    return;
  }
  PackingScene &scene = *bs.scene;

  BenchRegion r("coverage_baseline");

  CoverageParams params;
  params.dx = scene.dx;
  CoverageField field = BuildCoverageField(scene.container.mesh, scene.classGrid,
                                           scene.WorldOrigin(), params);
  CoverageReport rep = BuildCoverageReport(field);

  BerryVisibilityInput vis = CollectSmallInstances(scene);
  rep.visibleBerryFraction = ComputeVisibleBerryFraction(field, vis);

  // independence check: a second ray set that never drives targeting. A
  // large gap between the two open fractions means the primary set is
  // being gamed by its sampling, not that the surface actually improved.
  OrthoCoverageParams orthoParams;
  orthoParams.dx = scene.dx;
  CoverageReport orthoRep =
      BuildOrthoCoverageReport(scene.classGrid, scene.WorldOrigin(), orthoParams);

  std::ostringstream oss;
  oss << scene.instances.size() << " instances resumed, "
      << vis.smallInstanceCenters.size() << " small-kind\n";
  oss << "primary ray set:\n" << rep.toString();
  oss << "independent ortho ray set:\n" << orthoRep.toString();
  r.Note(oss.str());

  std::string mouthsFile = scene.outputFolder + "coverage_open_mouths.obj";
  SaveOpenMouthsObj(mouthsFile, field);
  Vec3u gsize = scene.classGrid.GetSize();
  SaveSlice(scene.outputFolder + "coverage_class_z1.png", scene.classGrid,
           gsize[2] / 3, 60.0f);
  SaveSlice(scene.outputFolder + "coverage_class_z2.png", scene.classGrid,
           2 * gsize[2] / 3, 60.0f);
  std::cout << "  saved " << mouthsFile << " and classGrid slices\n";

  r.Finish(&scene);
}

// H2/H3: runs PackPocketStep over every small kind and reports the
// coverage delta against the pre-step measurement, so this can be
// compared directly to coverage_baseline's numbers on the same pack file.
void BenchPocketFill(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, true);
  if (!bs.ok) {
    return;
  }
  PackingScene &scene = *bs.scene;

  BenchRegion r("pocket_fill");

  PackingStep step;
  for (const MeshInfo &item : scene.items) {
    Vec3f ext = item.box.vmax - item.box.vmin;
    float maxExtent = std::max({ext[0], ext[1], ext[2]});
    if (VoxClassOfExtent(maxExtent, VOX_SMALL_THRESH_CM) == VOX_ITEM_SMALL) {
      step.names.push_back(item.name);
    }
  }
  if (step.names.empty()) {
    std::cout << "  no small kinds in the catalogue, skipping\n";
    return;
  }

  CoverageParams beforeParams;
  beforeParams.dx = scene.dx;
  CoverageField beforeField = BuildCoverageField(scene.container.mesh, scene.classGrid,
                                                 scene.WorldOrigin(), beforeParams);
  CoverageReport beforeRep = BuildCoverageReport(beforeField);
  beforeRep.visibleBerryFraction =
      ComputeVisibleBerryFraction(beforeField, CollectSmallInstances(scene));

  PocketStepConfig cfg;
  cfg.coverage.dx = scene.dx;
  cfg.maxSecondsPerStep = std::min(ctx.RemainingSec(), 60.0f);
  cfg.maxBerries = ctx.itemCount * 20;

  size_t before = scene.instances.size();
  PocketStepResult result = PackPocketStep(scene, step, cfg);

  // same check smallfruit_packstep runs: any placement bug here (a bad
  // settle, a rejected-then-committed spot) has to show up as overlap or
  // outside-container depth, not just as a coverage number.
  if (ctx.validate && result.placed > 0) {
    PackValidator validator;
    validator.Init(scene, ctx.cfg);
    for (size_t i = 0; i < before; i++) {
      validator.AddPlaced(scene, scene.instances[i].itemId, scene.instances[i].tran);
    }
    unsigned failures = 0;
    float worstOverlap = 0.0f, worstOutside = 0.0f, overlapSum = 0.0f;
    for (size_t i = before; i < scene.instances.size(); i++) {
      const InstanceInfo &inst = scene.instances[i];
      ValidationResult vr = validator.ValidatePlacement(scene, inst.itemId, inst.tran);
      if (!vr.Ok()) {
        failures++;
      }
      overlapSum += vr.maxOverlapDepth;
      worstOverlap = std::max(worstOverlap, vr.maxOverlapDepth);
      worstOutside = std::max(worstOutside, vr.maxOutsideDepth);
      validator.AddPlaced(scene, inst.itemId, inst.tran);
    }
    std::ostringstream val;
    val << "validated " << result.placed << " new placements, " << failures
        << " failures, worst overlap " << worstOverlap << " cm, worst outside "
        << worstOutside << " cm, mean per-placement overlap "
        << (overlapSum / double(result.placed)) << " cm";
    r.Note(val.str());
  }

  CoverageReport afterRep = BuildCoverageReport(result.field);
  afterRep.visibleBerryFraction =
      ComputeVisibleBerryFraction(result.field, CollectSmallInstances(scene));

  std::ostringstream oss;
  oss << step.names.size() << " small kinds, " << result.placed << " placed in "
      << result.attempts << " attempts, exit: " << result.exitReason << "\n";
  oss << "rejects: no item contact " << result.rejectedNoContact << ", inward sink "
      << result.rejectedInwardSink << ", left patch " << result.rejectedLeftPatch
      << ", still open (approx) " << result.rejectedStillOpen << "\n";
  oss << "before:\n" << beforeRep.toString();
  oss << "after:\n" << afterRep.toString();
  if (result.placed > 0) {
    float raysClosedPerBerry =
        float(int(beforeRep.openRays) - int(afterRep.openRays)) / float(result.placed);
    oss << "open rays closed per berry: " << raysClosedPerBerry << "\n";
  }
  r.Note(oss.str());

  std::string mouthsFile = scene.outputFolder + "pocket_fill_open_mouths.obj";
  SaveOpenMouthsObj(mouthsFile, result.field);
  std::string packFile = scene.outputFolder + "pocket_fill_pack.txt";
  scene.SaveInstances(packFile);
  std::cout << "  saved " << mouthsFile << " and " << packFile << "\n";

  r.Finish(&scene);
}
