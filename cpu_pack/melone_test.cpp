#include "PackingConfig.h"
#include "PackingDriver.h"
#include "PackingPlan.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>

// Test case: pack fruits from fruits_1 into melone_two.obj as the
// container, using medium large (6-20 cm), medium small (3-6 cm), and
// small (<=3 cm) groups. The large (>20 cm) group is skipped since the
// ~14 cm melon container cannot hold them. The container mesh itself is
// excluded from the fruit list so it is not packed inside itself. Small
// fruits use the original lattice-walk strategy (useInnerContainer, inwards).
//
//   ./melone_test
//
// Outputs go to out_melone_test/ under the data directory.

int main() {
  std::cout.setf(std::ios::unitbuf);

  PackingConfig cfg;
  // cfg.dataDir = "/media/desaic/ssd2/meshes/fruit_hand/";
  //desktop
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

  cfg.maxTrialCount = 10;
  cfg.startStep = 0;
  cfg.startItem = 0;

  cfg.resume = true;
  cfg.resumePackFile = "melone_pack7.txt";
  cfg.trajSaveInterval = 10;
  cfg.packSaveInterval = 20;

  cfg.computeStats = true;
  cfg.maxSecondsPerStep = 60.0f;

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
  std::vector<float> SIZE_THRESH = {20, 6, 3};
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

  // Exclude the container mesh from the fruit groups, and filter out any
  // fruit whose max extent exceeds the container's -- a 20 cm banana will
  // never fit inside a 14 cm melon, and a single FindSpot call for a mesh
  // larger than the container can take minutes because the time check only
  // fires between items, not during the FFT search.
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
  step0.force = Vec3f(-10, 0, 0);
  plan.steps.push_back(step0);

  // Step 1: medium large full fill, outwards.
  PackingStep step1;
  step1.names = groups[1];
  step1.count = LARGE_INT;
  step1.outwards = true;
  plan.steps.push_back(step1);

  // Step 2: medium small, inwards, weak force.
  PackingStep step2;
  step2.names = groups[2];
  step2.count = LARGE_INT;
  step2.outwards = false;
  step2.force = Vec3f(-0.1f, 0, 0);
  plan.steps.push_back(step2);

  // Step 3: small fruits, original lattice-walk strategy. useInnerContainer
  // keeps berries out of the deep center, inwards with weak force.
  PackingStep lastStep;
  lastStep.names = groups[3];
  lastStep.outwards = false;
  lastStep.useInnerContainer = true;
  lastStep.count = LARGE_INT;
  lastStep.force = Vec3f(-0.1f, 0, 0);
  plan.steps.push_back(lastStep);

  if (plan.steps.empty()) {
    std::cout << "empty packing plan. nothing to do.\n";
    return 1;
  }

  PackFruits(plan, cfg);
  return 0;
}
