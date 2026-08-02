#include "PackingDriver.h"

#include "GridUtils.h"
#include "Log.h"
#include "MeshInfo.h"
#include "MeshOps.h"
#include "PackingOps.h"
#include "Profiler.h"
#include "Stopwatch.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

bool BuildScene(PackingScene &scene, const PackingConfig &cfg) {
  std::string meshDir = cfg.MeshDir();
  if (!fs::exists(meshDir)) {
    std::cout << "item directory missing " << meshDir << "\n";
    return false;
  }
  {
    PROFILE_SCOPE("init.load_items");
    scene.items = LoadAllMeshInfo(meshDir);
  }
  if (scene.items.empty()) {
    std::cout << "no items loaded from " << meshDir << "\n";
    return false;
  }

  fs::path containerFile(cfg.ContainerPath());
  if (LoadMeshInfo(scene.container, containerFile) != 0) {
    std::cout << "could not load container " << containerFile.string() << "\n";
    return false;
  }
  fs::path innerContainerFile(cfg.InnerContainerPath());
  if (LoadMeshInfo(scene.containerInner, innerContainerFile) != 0) {
    // inner container is optional. steps that need it will skip it.
    LOGI("no inner container at " << innerContainerFile.string() << "\n");
  }

  scene.dx = cfg.dx;
  scene.gridDx = cfg.gridDx;
  scene.containerSDFDx = cfg.containerSDFDx;
  scene.subgridCellSize = cfg.subgridCellSize;
  scene.outputFolder = cfg.OutputFolder();

  scene.broadPhase.Init(scene.container.box, cfg.broadPhaseDx);
  scene.InitDataStructures();
  return true;
}

void PrepareBackground(PackingScene &scene, const PackingConfig &cfg) {
  PROFILE_SCOPE("init.bg_voxelize");
  scene.dx = cfg.dx;
  scene.bg.SetMeshPtr(&scene.container.mesh);
  scene.bg.Voxelize(scene.dx);
  InvertContainer(scene.bg.vox, 1);
}

void PackStep(PackingScene &scene, const PackingStep &step, const PackingConfig &cfg) {
  unsigned count = 0;
  // first item to consider in the next iteration.
  unsigned startNameIndex = 0;
  if (step.names.size() == 0) {
    return;
  }

  const unsigned MAX_TRIAL_COUNT = cfg.maxTrialCount;
  unsigned angleIndex = 0;
  unsigned numItems = step.names.size();
  float sdfFactor = step.outwards ? 1.0f : -1.0f;
  if (step.useInnerContainer) {
    AddInnerContainer(scene);
  }

  // checked between items rather than inside the trial loop, so a step
  // overruns by at most one placement attempt.
  Utils::Stopwatch stepClock;
  stepClock.Start();
  bool outOfTime = false;
  auto timeUp = [&]() {
    if (cfg.maxSecondsPerStep <= 0.0f) {
      return false;
    }
    if (stepClock.ElapsedMS() > 1000.0 * double(cfg.maxSecondsPerStep)) {
      outOfTime = true;
      return true;
    }
    return false;
  };

  for (; count < step.count && !outOfTime; count++) {
    bool packSuccess = false;
    for (unsigned i = cfg.startItem; i < numItems && !timeUp(); i++) {
      unsigned nameIndex = (i + startNameIndex) % numItems;
      std::string name = step.names[nameIndex];

      unsigned itemIndex = scene.GetItemIndex(name);
      MeshInfo &item = scene.items[itemIndex];
      if (item.noMoreFit) {
        continue;
      }
      Vec3f itemExtent = item.box.vmax - item.box.vmin;
      float itemMaxExtent = std::max({itemExtent[0], itemExtent[1], itemExtent[2]});
      unsigned totalCells = scene.numSubgridCells[0] * scene.numSubgridCells[1]
                            * scene.numSubgridCells[2];
      bool useSubgrid = (totalCells > 0 && itemMaxExtent < scene.subgridCellSize);
      bool ignoreCellBoundary = false;

      auto placeItem = [&](const Vec3f &p, const Vec3f &r) {
        packSuccess = true;
        RigidTransform tran;
        tran.position = p;
        tran.rotation = RotationMatrixRad(r[0], r[1], r[2]);
        Vec3f pushDir = scene.ForceDirection(itemIndex, step.force, sdfFactor, tran);
        LOGI(scene.items[itemIndex].name << " " << pushDir[0] << " " << pushDir[1]
                                        << " " << pushDir[2] << "\n");
        std::vector<RigidTransform> trajectory;
        RigidTransform newTran = scene.Nudge(itemIndex, tran, pushDir, trajectory);
        unsigned instanceId = scene.Put(itemIndex, newTran);
        scene.instances[instanceId].trajectory = trajectory;
        if (cfg.trajSaveInterval > 0 && count % cfg.trajSaveInterval == 0 && count > 0) {
          std::string trajFile = scene.trajFile
                                 + std::to_string(int(count / cfg.trajSaveInterval) % 10)
                                 + ".txt";
          scene.SaveTrajectories(trajFile);
        }
        if (cfg.packSaveInterval > 0 && count % cfg.packSaveInterval == 0 && count > 0) {
          std::string packFile = scene.packFile
                                 + std::to_string(int(count / cfg.packSaveInterval) % 10)
                                 + ".txt";
          scene.SaveInstances(packFile);
        }
      };

      bool itemPlaced = false;
      if (useSubgrid) {
        // one item can walk all 90 cells at 10 trials each, so the cell
        // loop needs its own check or a single item could blow the budget.
        while (!itemPlaced && item.nextCellIdx < totalCells && !timeUp()) {
          unsigned cellIdx = item.nextCellIdx;
          item.nextCellIdx++;
          bool cellSuccess = false;
          for (unsigned trial = 0; trial < MAX_TRIAL_COUNT; trial++) {
            Vec3f pos;
            Vec3f rot = scene.randAngles[angleIndex];
            angleIndex++;
            if (angleIndex >= scene.randAngles.size()) {
              angleIndex = 0;
            }
            if (FindSpotSubgrid(scene.bg, item.mesh, pos, rot, scene.sdf,
                                sdfFactor, scene.subgridCellSize,
                                cellIdx, scene.numSubgridCells,
                                ignoreCellBoundary)) {
              placeItem(pos, rot);
              itemPlaced = true;
              cellSuccess = true;
              break;
            }
          }
          if (cellSuccess) break;
        }
        if (!itemPlaced && item.nextCellIdx >= totalCells) {
          item.noMoreFit = true;
        }
      } else {
        for (unsigned trial = 0; trial < MAX_TRIAL_COUNT; trial++) {
          Vec3f pos;
          Vec3f rot = scene.randAngles[angleIndex];
          angleIndex++;
          if (angleIndex >= scene.randAngles.size()) {
            angleIndex = 0;
          }
          if (FindSpot(scene.bg, item.mesh, pos, rot, scene.sdf, sdfFactor)) {
            placeItem(pos, rot);
            itemPlaced = true;
            break;
          }
        }
      }
      if (!itemPlaced && (!useSubgrid || item.nextCellIdx >= totalCells)) {
        item.noMoreFit = true;
      }
    }
    if (!packSuccess) {
      bool anySubgridRemaining = false;
      unsigned tCells = scene.numSubgridCells[0] * scene.numSubgridCells[1]
                        * scene.numSubgridCells[2];
      for (unsigned nameIndex = 0; nameIndex < numItems; nameIndex++) {
        unsigned itemIdx = scene.GetItemIndex(step.names[nameIndex]);
        MeshInfo &it = scene.items[itemIdx];
        if (tCells > 0 && it.nextCellIdx < tCells) {
          anySubgridRemaining = true;
          break;
        }
      }
      if (!anySubgridRemaining) {
        break;
      }
      packSuccess = true;
    }
    startNameIndex = (startNameIndex + 1) % numItems;
  }
}

void PackScene(PackingScene &scene, const PackingPlan &plan, const PackingConfig &cfg) {
  PrepareBackground(scene, cfg);

  scene.packFile = scene.outputFolder + "/pack";
  scene.trajFile = scene.outputFolder + "/traj";
  scene.placed.resize(scene.items.size());

  if (cfg.resume) {
    LoadPack(scene, cfg.ResumePackPath());
  }

  for (size_t i = cfg.startStep; i < plan.steps.size(); i++) {
    PackStep(scene, plan.steps[i], cfg);
  }
}

void PackFruits(const PackingPlan &plan, const PackingConfig &cfg) {
  PackingScene scene;
  if (!BuildScene(scene, cfg)) {
    return;
  }
  PackScene(scene, plan, cfg);
}
