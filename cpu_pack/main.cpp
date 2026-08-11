#include "Log.h"
#include "PackingConfig.h"
#include "PackingDriver.h"
#include "PackingPlan.h"

#include <iostream>

int main(int argc, char *argv[]) {

  PackingConfig cfg;
  cfg.ParseArgs(argc, argv);
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
  PackFruits(plan, cfg);
  return 0;
}
