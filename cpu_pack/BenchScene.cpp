#include "BenchScene.h"

#include "Log.h"
#include "PackingDriver.h"

#include <iostream>

BenchScene MakeBenchScene(const PackingConfig &cfg, bool loadPack) {
  BenchScene bs;
  PackingConfig local = cfg;
  // periodic saves rewrite the whole accumulated state, so they would
  // dominate any timing here.
  local.trajSaveInterval = 0;
  local.packSaveInterval = 0;
  local.computeStats = false;

  bs.scene = std::make_unique<PackingScene>();
  if (!BuildScene(*bs.scene, local)) {
    std::cout << "  setup failed, skipping\n";
    return bs;
  }
  PrepareBackground(*bs.scene, local);
  bs.scene->packFile = bs.scene->outputFolder + "/bench_pack";
  bs.scene->trajFile = bs.scene->outputFolder + "/bench_traj";
  bs.scene->placed.resize(bs.scene->items.size());

  if (loadPack) {
    LoadPack(*bs.scene, local.ResumePackPath());
    if (bs.scene->instances.empty()) {
      std::cout << "  warning: no instances loaded from "
                << local.ResumePackPath() << ", container is empty\n";
    }
  }

  bs.bySize = SortBySize(bs.scene->items);
  bs.ok = true;
  return bs;
}

std::vector<unsigned> PickBigItems(const BenchScene &bs, unsigned n) {
  std::vector<unsigned> out;
  for (size_t i = 0; i < bs.bySize.size() && out.size() < n; i++) {
    out.push_back(unsigned(bs.bySize[i]));
  }
  return out;
}

std::vector<unsigned> PickSmallItems(const BenchScene &bs, unsigned n) {
  std::vector<unsigned> out;
  for (size_t i = bs.bySize.size(); i > 0 && out.size() < n; i--) {
    out.push_back(unsigned(bs.bySize[i - 1]));
  }
  return out;
}
