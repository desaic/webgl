#include "PackingConfig.h"
#include "PackingDriver.h"
#include "PackingScene.h"

#include <iostream>

// Standalone harness to debug false-positive "deep rays": builds the
// scene, loads the resume pack (same one melone_test uses), then
// inspects one specific ray without running any packing steps.
//
//   ./debug_ray_test
//
// Edit targetPos below to inspect a different ray.

int main() {
  std::cout.setf(std::ios::unitbuf);

  PackingConfig cfg;
  cfg.dataDir = "/media/desaic/WD/meshes/fruit_hand/";
  cfg.fruitSubdir = "fruits_1";
  cfg.containerFile = "fruits_1/melone_two.obj";
  cfg.innerContainerFile = "hands/finger_inner4.8m.stl";
  cfg.outSubdir = "out_melone_test";

  cfg.dx = 0.3f;
  cfg.containerSDFDx = 2.0f;
  cfg.broadPhaseDx = 2.0f;
  cfg.gridDx = 0.5f;
  cfg.subgridCellSize = 20.0f;

  cfg.resume = true;
  cfg.resumePackFile = "melone_pack7.txt";

  PackingScene scene;
  if (!BuildScene(scene, cfg)) {
    std::cout << "failed to build scene\n";
    return 1;
  }
  PrepareBackground(scene, cfg);
  LoadPack(scene, cfg.ResumePackPath());

  Vec3f targetPos(6.45088f, -4.37227f, 0.386937f);
  DebugDeepRayNeighbors(scene, targetPos);

  std::vector<Vec3f> deepOrigins, deepEnds;
  ComputeAndSaveSurfaceDepths(scene, deepOrigins, deepEnds);

  return 0;
}
