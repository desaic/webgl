#include "PackingConfig.h"
#include "PackingDriver.h"
#include "PackingPlan.h"
#include "DebugTools.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>

// Test case: melone_two.obj container
// using medium large (6-20 cm), medium small (3-6 cm), and
// small (<=3 cm) groups. Small
// fruits use the original voxel strategy (useInnerContainer, inwards).
//
//   ./melone_test
//
// Outputs go to out_melone_test/ under the data directory.

int main() {
  std::cout.setf(std::ios::unitbuf);

  PackingConfig cfg;
#ifdef _WIN32
  cfg.dataDir = "F:/meshes/fruit_hand/";
#else
  // cfg.dataDir = "/media/desaic/ssd2/meshes/fruit_hand/";
  cfg.dataDir = "/media/desaic/WD/meshes/fruit_hand/";
#endif
  cfg.fruitSubdir = "fruits_1";
  cfg.containerFile = "fruits_1/melone_two.obj";
  cfg.innerContainerFile = "hands/melone_two_inner.obj";
  cfg.outSubdir = "out_melone_test";

  cfg.dx = 0.3f;
  cfg.containerSDFDx = 2.0f;
  cfg.broadPhaseDx = 2.0f;
  cfg.gridDx = 0.5f;
  cfg.subgridCellSize = 20.0f;

  cfg.maxTrialCount = 10;
  // rewind: resume from the Phase 1 pack and skip all 4 steps.
  cfg.startStep = 4;
  cfg.startItem = 0;

  cfg.resume = true;
  cfg.resumePackFile = "out_melone_test/pack_dbg_phase1.txt";
  cfg.trajSaveInterval = 10;
  cfg.packSaveInterval = 20;

  cfg.computeStats = true;
  DebugShrinkWrap(cfg, 1, 0.2);
  DebugVoidField(cfg, 1, 0.1);
  DebugVoidSpots(cfg, 1, 0.1);
  DebugFillVoids(cfg, 1, 0.1);
  // DebugVoidSpots above IS Phase 2's own debug view now too -- see
  // DebugTools.h's note on the removed DebugCreviceField.
  std::cout << cfg.toString();

  // The output directory is not created by PackingScene -- create it here
  // so periodic saves don't silently fail.
  std::error_code ec;
  std::filesystem::create_directories(cfg.OutputFolder(), ec);

  std::string meshDir = cfg.MeshDir();
  if (cfg.computeStats) {
    ComputeMeshStats(meshDir);
  }

  std::vector<MeshStat> stats = LoadMeshStats(meshDir);
  if (stats.empty()) {
    std::cout << "no meshes found in " << meshDir << "\n";
    return 1;
  }

  // Group by size: [0] large >20, [1] medium large 6-20, [2] medium small
  // 3-6, [3] small <=3. Same thresholds as PlanPackingSteps.
  std::vector<float> SIZE_THRESH = {20, 6, 3, 2};
  std::vector<std::vector<std::string>> groups(SIZE_THRESH.size() + 1);
  std::map<std::string, float> nameToLen;
  for (const auto &s : stats) {
    float len = s.MaxExtent();
    nameToLen[s.name] = len;
    unsigned gid = GetGroupIndex(len, SIZE_THRESH);
    groups[gid].push_back(s.name);
  }

  // Largest first within each group.
  for (auto &g : groups) {
    std::sort(g.begin(), g.end(),
              [&](const std::string &a, const std::string &b) {
                return nameToLen[a] > nameToLen[b];
              });
  }

  // Exclude the container mesh from the fruit groups
  // filter out big fruits.
  const std::string containerName = "melone_two";
  float containerExtent = 0.0f;
  for (const auto &s : stats) {
    if (s.name == containerName) {
      containerExtent = s.MaxExtent();
      break;
    }
  }
  for (auto &g : groups) {
    g.erase(std::remove(g.begin(), g.end(), containerName), g.end());
    g.erase(std::remove_if(g.begin(), g.end(),
                           [&](const std::string &n) {
                             return nameToLen[n] > containerExtent * 0.9f;
                           }),
            g.end());
  }

  PackingPlan plan;
  plan.groups = groups;
  const unsigned LARGE_INT = 1000000u;

  // Step 0: seed medium large towards the left.
  PackingStep step0;
  step0.names = groups[1];
  step0.count = 20;
  step0.force = Vec3f(-1, 0, 0);
  step0.biasW = 10;
  plan.steps.push_back(step0);

  // Step 1: medium large full fill, outwards.
  PackingStep step1;
  step1.names = groups[1];
  step1.count = LARGE_INT;
  step1.outwards = true;
  plan.steps.push_back(step1);

  // Step 2: medium small, inwards, weak bias force.
  PackingStep step2;
  step2.names = groups[2];
  step2.count = LARGE_INT;
  step2.outwards = false;
  step2.force = Vec3f(1.0f, 0, 0);
  step2.biasW = 0.1f;
  plan.steps.push_back(step2);

  // Step 3: small fruits, original voxel strategy. useInnerContainer
  // keeps berries out of the deep center, inwards with weak force.
  PackingStep step3;
  step3.names = groups[3];
  step3.outwards = false;
  step3.useInnerContainer = true;
  step3.count = LARGE_INT;
  step3.biasW = 0.1f;
  step3.useFreeSurfacePoints = true;
  plan.steps.push_back(step3);

  // Step 4: Phase 2 crevice fill, small fruit (groups.size()-2, same tier
  // step3 already placed in bulk). cfg.startStep=4 (see the rewind-state
  // config above) starts the run right here, resuming from a Phase-1-only
  // pack.
  PackingStep step4;
  step4.kind = StepKind::Crevice;
  step4.names = groups[groups.size() - 2];
  step4.shrinkwrapRadius = 1.0f;
  step4.shrinkwrapVoxelSize = 0.2f;
  step4.shrinkwrapOpenRadiusVoxels = 1;
  plan.steps.push_back(step4);

  // Step 5: Phase 2 crevice fill, tiniest fruit tier -- same grid, finer
  // fruit tier than step4.
  PackingStep step5;
  step5.kind = StepKind::Crevice;
  step5.names = groups.back();
  step5.shrinkwrapRadius = 1.0f;
  step5.shrinkwrapVoxelSize = 0.2f;
  step5.shrinkwrapOpenRadiusVoxels = 1;
  plan.steps.push_back(step5);

  if (plan.steps.empty()) {
    std::cout << "empty packing plan. nothing to do.\n";
    return 1;
  }

  PackFruits(plan, cfg);
  return 0;
}
