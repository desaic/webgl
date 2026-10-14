#include "DebugTools.h"
#include "Log.h"
#include "PackingConfig.h"
#include "PackingDriver.h"
#include "PackingPlan.h"

#include "MediumGraph.h"
#include "MeshOps.h"

#include <iostream>
#include <filesystem>
#include <limits>

// Standalone inspection; run cpu_pack --debug-pack-crevices to skip packing.
// Radius matches the medium placement step, with finer voxels for crevices.
void ComputePackShrinkwrapAndCrevices(
    PackingConfig cfg,
    const std::string &packFile = "F:/meshes/fruit_hand/pack_filtered_step2.txt",
    float shrinkRadius = 3.0f, float voxelSize = 0.5f) {
  const auto packPath = std::filesystem::absolute(packFile);
  cfg.resume = true;
  cfg.resumePackFile = packPath.string();
  cfg.outSubdir = packPath.parent_path().string();
  // Inspect all opening sizes, rather than the small-fruit pass's <=3cm set.
  cfg.creviceMaxWidth = std::numeric_limits<float>::max();
  CreviceFieldParams fp;
  fp.shrinkwrapRadius = shrinkRadius;
  fp.shrinkwrapVoxelSize = voxelSize;
  fp.shrinkwrapOpenRadiusVoxels = 1;
  DebugCreviceSpots(cfg, fp, false);
}

int main(int argc, char *argv[]) {
  // TrigMesh container;
  // container.LoadStl("F:/meshes/fruit_hand/hands/hand4.8m.stl");
  // std::cout << " loaded " << container.GetNumVerts() << " " << container.GetNumTrigs() << "\n";
  // TrigMesh inner = ComputeInnerSurface(container, 2, 12);
  // inner.SaveObj("F:/meshes/fruit_hand/hands/hand4.8m_inner12cm.obj");
  // std::cout << " done \n\n\n";

  PackingConfig cfg;
  const char * argvDebug [2] = {"", "F:/github/webgl/cpu_pack/configs/pack_hand.cfg"};
  // A supplied config is respected; the hand config remains the convenient
  // default for local one-off runs and the debug modes below.
  if (argc > 1 && std::string(argv[1]).rfind("--", 0) != 0)
    cfg.ParseArgs(argc, argv);
  else
    cfg.ParseArgs(2, (char**)argvDebug);
  if (argc > 1 && std::string(argv[1]) == "--debug-crevice-sweep") {
    std::cout.setf(std::ios::unitbuf);
    SetLogLevel(LOG_INFO);
    cfg.resumePackFile = "F:/meshes/fruit_hand/pack_filtered_step2.txt";
    cfg.outSubdir = "crevice_radius_sweep";
    cfg.creviceMaxWidth = std::numeric_limits<float>::max();
    return DebugCreviceRadiusSweep(cfg, {3.0f, 6.0f, 12.0f}, 1.0f) ? 0 : 1;
  }
  if (argc > 1 && std::string(argv[1]) == "--debug-pack-crevices") {
    std::cout.setf(std::ios::unitbuf);
    SetLogLevel(LOG_INFO);
    ComputePackShrinkwrapAndCrevices(cfg);
    return 0;
  }
  if (argc > 1 && std::string(argv[1]) == "--compute-stats") {
    // rewrites stats.txt (bounding box and bounding sphere radius) and stops.
    ComputeMeshStats(cfg.MeshDir());
    SaveSizeGroups(cfg.MeshDir(), cfg.MeshDir() + "/size_groups.txt");
    return 0;
  }
  std::cout << cfg.toString();

  std::string meshDir = cfg.MeshDir();
  if (cfg.computeStats) {
    ComputeMeshStats(meshDir);
  }
  PackingPlan plan = PlanPackingSteps(meshDir);
  if (plan.steps.empty()) {
    std::cout << "empty packing plan. nothing to do.\n";
    return 1;
  }
  if (argc > 1 && std::string(argv[1]) == "--medium-graph") {
    // graph of placed 8 to 14 cm fruit with x < sphereMaxX, from cfg.resumePackFile.
    SetLogLevel(LOG_INFO);
    if (argc > 2) cfg.ParseArgs(argc - 1, argv + 1);
    return RunMediumGraph(cfg, plan.groups[2]) ? 0 : 1;
  }
  if (cfg.oldWayMedium) {
    // one bulk step on the 8 to 14 cm group (plan group 2): FFT spot search on
    // the eroded fruit, then Nudge toward the container interior.
    PackingStep step;
    step.names = plan.groups[2];
    step.count = 1000000u;
    step.erodeVoxels = cfg.oldWayErode;
    step.nudgeInward = true;
    step.overlapHeuristic = cfg.oldWayOverlap;
    plan.steps.clear();
    plan.steps.push_back(step);
    cfg.startStep = 0;
    cfg.endStep = 1;
  }
  SetLogLevel(LOG_INFO);
  PackFruits(plan, cfg);
  return 0;
}
