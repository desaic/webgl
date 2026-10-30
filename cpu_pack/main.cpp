#include "DebugTools.h"
#include "Log.h"
#include "PackingConfig.h"
#include "PackingDriver.h"
#include "PackingPlan.h"

#include "ClearanceGate.h"
#include "MediumGraph.h"
#include "CavityRefine.h"
#include "InnerForce.h"
#include "Vhacd.h"
#include "MeshOps.h"

#include <algorithm>
#include <iostream>
#include <sstream>
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
  std::cout.setf(std::ios::unitbuf);
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
  if (argc > 1 && (std::string(argv[1]) == "--medium-graph" ||
                    std::string(argv[1]) == "--medium-nudge")) {
    // graph of placed 8 to 14 cm fruit with x < sphereMaxX, from cfg.resumePackFile.
    std::cout.setf(std::ios::unitbuf);
    SetLogLevel(LOG_INFO);
    if (argc > 2) cfg.ParseArgs(argc - 1, argv + 1);
    return RunMediumGraph(cfg, plan.groups[2], std::string(argv[1]) == "--medium-nudge") ? 0 : 1;
  }
  if (argc > 1 && (std::string(argv[1]) == "--cavity-metric" || std::string(argv[1]) == "--cavity-refine")) {
    std::cout.setf(std::ios::unitbuf);
    SetLogLevel(LOG_INFO);
    if (argc > 2) cfg.ParseArgs(argc - 1, argv + 1);
    return RunCavityRefine(cfg, plan.groups[cfg.oldWayGroup], std::string(argv[1]) == "--cavity-metric") ? 0 : 1;
  }
  if (argc > 1 && std::string(argv[1]) == "--vhacd") {
    std::cout.setf(std::ios::unitbuf);
    SetLogLevel(LOG_INFO);
    if (argc > 2) cfg.ParseArgs(argc - 1, argv + 1);
    return RunVhacd(cfg) ? 0 : 1;
  }
  if (argc > 1 && (std::string(argv[1]) == "--inner-force" || std::string(argv[1]) == "--inner-nudge" || std::string(argv[1]) == "--inner-free" || std::string(argv[1]) == "--hull-check" || std::string(argv[1]) == "--hull-pair" || std::string(argv[1]) == "--sdf-check" || std::string(argv[1]) == "--deep-compare" || std::string(argv[1]) == "--filter-touch")) {
    std::cout.setf(std::ios::unitbuf);
    SetLogLevel(LOG_INFO);
    if (argc > 2) cfg.ParseArgs(argc - 1, argv + 1);
    if (std::string(argv[1]) == "--inner-free") return RunInnerFree(cfg) ? 0 : 1;
    if (std::string(argv[1]) == "--hull-check") return RunHullCheck(cfg) ? 0 : 1;
    if (std::string(argv[1]) == "--filter-touch") {
      // --filter-touch cfg packIn packOut [tol minTouch]
      if (argc < 5) return 1;
      return RunFilterTouch(cfg, argv[3], argv[4], argc > 5 ? float(atof(argv[5])) : 0.5f, argc > 6 ? unsigned(atoi(argv[6])) : 2u) ? 0 : 1;
    }
    if (std::string(argv[1]) == "--deep-compare") {
      // --deep-compare cfg watermelonId packA packB id id id ...
      if (argc < 7) return 1;
      std::vector<unsigned> ids;
      for (int i = 6; i < argc; i++) ids.push_back(unsigned(atoi(argv[i])));
      return RunDeepCompare(cfg, {argv[4], argv[5]}, ids, unsigned(atoi(argv[3]))) ? 0 : 1;
    }
    if (std::string(argv[1]) == "--sdf-check") {
      if (argc < 12) return 1;
      return RunSdfCheck(cfg, argv[3], argv[4], Vec3f(float(atof(argv[5])), float(atof(argv[6])), float(atof(argv[7]))), argv[8],
                         Vec3f(float(atof(argv[9])), float(atof(argv[10])), float(atof(argv[11])))) ? 0 : 1;
    }
    if (std::string(argv[1]) == "--hull-pair") {
      // --hull-pair cfg pack nameA x y z nameB x y z
      if (argc < 12) return 1;
      return RunHullPair(cfg, argv[3], argv[4], Vec3f(float(atof(argv[5])), float(atof(argv[6])), float(atof(argv[7]))), argv[8],
                         Vec3f(float(atof(argv[9])), float(atof(argv[10])), float(atof(argv[11])))) ? 0 : 1;
    }
    return RunInnerForce(cfg, std::string(argv[1]) == "--inner-nudge") ? 0 : 1;
  }
  if (cfg.oldWayMedium) {
    // one bulk step on one size group (cfg.oldWayGroup, default 2: 8 to 14 cm): FFT spot search on
    // the eroded fruit, then Nudge toward the container interior.
    PackingStep step;
    if (cfg.oldWayGroup >= plan.groups.size()) return 1;
    step.names = plan.groups[cfg.oldWayGroup];
    if (!cfg.oldWayKinds.empty()) {
      std::vector<std::string> keep;
      std::stringstream ss(cfg.oldWayKinds);
      for (std::string name; std::getline(ss, name, ',');) {
        if (std::find(step.names.begin(), step.names.end(), name) != step.names.end()) keep.push_back(name);
        else std::cout << "oldWayKinds: " << name << " is not in the oldWayGroup size group, ignored" << std::endl;
      }
      step.names = keep;
    }
    step.count = 1000000u;
    step.erodeVoxels = cfg.oldWayErode;
    step.nudgeInward = true;
    step.useInnerContainer = cfg.oldWayInner;
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
