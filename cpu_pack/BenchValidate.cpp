#include "BenchReport.h"
#include "BenchScene.h"
#include "Benchmark.h"
#include "PackValidate.h"

#include <iostream>
#include <sstream>

// a validator that never reports a failure is indistinguishable from one
// that is broken, so this feeds it three placements with known answers:
// one well outside the container, one on top of an existing item, and one
// taken straight from the saved pack. only the third should pass.
void BenchValidateSelfTest(BenchContext &ctx) {
  BenchScene bs = MakeBenchScene(ctx.cfg, true);
  if (!bs.ok) {
    return;
  }
  PackingScene &scene = *bs.scene;
  if (scene.instances.empty()) {
    std::cout << "  no placed instances to test against, skipping\n";
    return;
  }

  PackValidator v;
  v.Init(scene, ctx.cfg);
  v.AddAllPlaced(scene);

  unsigned checks = 0, correct = 0;
  auto expect = [&](const std::string &label, const ValidationResult &res,
                    bool wantOk) {
    checks++;
    bool got = res.Ok();
    bool pass = (got == wantOk);
    if (pass) {
      correct++;
    }
    std::cout << "  " << (pass ? "pass" : "FAIL") << "  " << label
              << "  expected " << (wantOk ? "OK" : "violation") << ", got "
              << res.toString() << "\n";
  };

  const InstanceInfo &ref = scene.instances[0];
  unsigned itemIdx = ref.itemId;

  // 1. shoved hard against the container wall, but still inside the
  // validation grid, so this exercises the exterior-marked-occupied path
  // rather than just falling off the grid.
  Vec3f extent = scene.container.box.vmax - scene.container.box.vmin;
  RigidTransform wall = ref.tran;
  wall.position = ref.tran.position;
  wall.position[1] = scene.container.box.vmax[1] - 0.02f * extent[1];
  expect("pushed through container wall",
         v.ValidatePlacement(scene, itemIdx, wall), false);

  // 2. fully off the grid. should be caught by offGridCount, not silently
  // pass because no occupied voxel was ever looked up.
  RigidTransform faraway = ref.tran;
  faraway.position = scene.container.box.vmax + extent;
  expect("far outside the grid",
         v.ValidatePlacement(scene, itemIdx, faraway), false);

  // 3. exactly on top of an item already in the fruit grid. this is the
  // weakest case the test has: a duplicate's surface samples sit on the
  // neighbor's surface, so almost none of them read as deep. it only
  // clears the threshold by a handful of samples.
  expect("exact duplicate of placed item",
         v.ValidatePlacement(scene, itemIdx, ref.tran), false);

  // 4. shifted halfway into that same neighbor, so a whole face of samples
  // sits well inside it. this is what a real overlap bug looks like and
  // gives the threshold a wide margin to work with.
  const MeshInfo &refItem = scene.items[itemIdx];
  Vec3f itemExtent = refItem.box.vmax - refItem.box.vmin;
  RigidTransform halfIn = ref.tran;
  halfIn.position[0] += 0.4f * itemExtent[0];
  expect("shifted halfway into placed item",
         v.ValidatePlacement(scene, itemIdx, halfIn), false);

  // 5. a real placement, checked against a fruit grid that excludes it.
  // rebuild the grid without instance 0 so its own voxels do not count
  // as an overlap with itself.
  PackValidator clean;
  clean.Init(scene, ctx.cfg);
  for (size_t i = 1; i < scene.instances.size(); i++) {
    clean.AddPlaced(scene, scene.instances[i].itemId, scene.instances[i].tran);
  }
  expect("real placement from pack file",
         clean.ValidatePlacement(scene, itemIdx, ref.tran), true);

  BenchRegion r("validate_selftest");
  std::ostringstream oss;
  oss << correct << " of " << checks << " expectations met";
  r.Note(oss.str());
  oss.str("");
  oss << "validator grids " << (v.MemoryBytes() / (1024 * 1024)) << " MB";
  r.Note(oss.str());
  r.Finish();

  if (correct != checks) {
    std::cout << "  validator is not trustworthy, treat other validate"
                 " results with suspicion\n";
  }
}
