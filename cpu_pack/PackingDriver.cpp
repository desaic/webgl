#include "PackingDriver.h"

#include "GridUtils.h"
#include "Log.h"
#include "MeshInfo.h"
#include "MeshOps.h"
#include "PackingOps.h"
#include "Profiler.h"
#include "Stopwatch.h"

#include <algorithm>
#include <cmath>
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

  // a previous step packed with a different force direction and left the
  // container at a different occupancy, so "no more fit" from back then is
  // not evidence about this step. medium items appear in two steps.
  for (unsigned i = 0; i < numItems; i++) {
    MeshInfo &it = scene.items[scene.GetItemIndex(step.names[i])];
    it.noMoreFit = false;
    it.nextCellIdx = 0;
  }

  float sdfFactor = step.outwards ? 1.0f : -1.0f;
  if (step.useInnerContainer) {
    AddInnerContainer(scene);
  }

  // checked between items rather than inside the trial loop, so a step
  // overruns by at most one placement attempt.
  Utils::Stopwatch stepClock;
  stepClock.Start();
  bool outOfTime = false;

  unsigned totalStepCells = scene.numSubgridCells[0] * scene.numSubgridCells[1]
                            * scene.numSubgridCells[2];
  LOGI("pack step: " << numItems << " kinds, target " << step.count
                     << ", force " << step.force[0] << " " << step.force[1]
                     << " " << step.force[2] << ", outwards " << step.outwards
                     << ", innerContainer " << step.useInnerContainer << "\n");
  if (numItems <= 16) {
    LOGI("  kinds:");
    for (unsigned i = 0; i < numItems; i++) {
      LOGI(" " << step.names[i]);
    }
    LOGI("\n");
  }

  // steps hold different numbers of kinds, so a startItem that is valid for
  // one step can be past the end of another. left unclamped the item loop
  // body never runs and the step spins through step.count rounds placing
  // nothing.
  unsigned startItem = cfg.startItem;
  if (startItem >= numItems) {
    LOGI("  startItem " << startItem << " past the last of " << numItems
                        << " kinds in this step, clamped to "
                        << (numItems - 1) << "\n");
    startItem = numItems - 1;
  }

  // a saturated container burns thousands of failed searches without a
  // single placement, and the only output used to come from placeItem, so
  // that case looked exactly like a hang. these counters print regardless
  // of whether anything is being placed.
  unsigned long searches = 0;
  unsigned placedCount = 0;
  double lastReportMs = 0.0;
  const double REPORT_INTERVAL_MS = 2000.0;
  auto report = [&](const char *tag) {
    unsigned retired = 0, cursorSum = 0;
    for (unsigned i = 0; i < numItems; i++) {
      const MeshInfo &it = scene.items[scene.GetItemIndex(step.names[i])];
      if (it.noMoreFit) {
        retired++;
      }
      cursorSum += it.nextCellIdx;
    }
    LOGI("  " << tag << " " << (stepClock.ElapsedMS() / 1000.0) << " s, "
              << searches << " searches, " << placedCount << " placed, "
              << retired << "/" << numItems << " kinds retired, "
              << cursorSum << "/" << (numItems * totalStepCells)
              << " cell slots walked, " << scene.instances.size()
              << " instances\n");
    lastReportMs = stepClock.ElapsedMS();
  };
  auto heartbeat = [&]() {
    if (stepClock.ElapsedMS() - lastReportMs > REPORT_INTERVAL_MS) {
      report("progress");
    }
  };
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
    for (unsigned i = startItem; i < numItems && !timeUp(); i++) {
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
        double settleStartMs = stepClock.ElapsedMS();
        std::vector<RigidTransform> trajectory;
        RigidTransform newTran = scene.Nudge(itemIndex, tran, pushDir, trajectory);
        unsigned instanceId = scene.Put(itemIndex, newTran);
        scene.instances[instanceId].trajectory = trajectory;
        placedCount++;
        // the found spot and the settled spot both matter: a large gap
        // between them means the search is aiming at places the nudge has
        // to drag the item out of.
        Vec3f moved = newTran.position - p;
        LOGI("  placed " << scene.items[itemIndex].name << " instance "
                         << instanceId << " cell " << item.nextCellIdx
                         << " at " << newTran.position[0] << " "
                         << newTran.position[1] << " " << newTran.position[2]
                         << ", settled " << moved.norm() << " cm in "
                         << (stepClock.ElapsedMS() - settleStartMs) << " ms, "
                         << placedCount << " this step\n");
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
          bool cellSuccess = false;
          for (unsigned trial = 0; trial < MAX_TRIAL_COUNT; trial++) {
            Vec3f pos;
            Vec3f rot = scene.randAngles[angleIndex];
            angleIndex++;
            if (angleIndex >= scene.randAngles.size()) {
              angleIndex = 0;
            }
            searches++;
            heartbeat();
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
          if (cellSuccess) {
            // stay on this cell. a 20cm cell holds many small items, so
            // advancing on success would cap the run at one placement per
            // (item, cell) pair, i.e. 90 per kind.
            break;
          }
          // only a cell that failed every trial is retired.
          item.nextCellIdx++;
        }
        // the retirement test below covers this path identically, so the
        // duplicate that used to sit here was removed.
      } else {
        for (unsigned trial = 0; trial < MAX_TRIAL_COUNT; trial++) {
          Vec3f pos;
          Vec3f rot = scene.randAngles[angleIndex];
          angleIndex++;
          if (angleIndex >= scene.randAngles.size()) {
            angleIndex = 0;
          }
          searches++;
          heartbeat();
          if (FindSpot(scene.bg, item.mesh, pos, rot, scene.sdf, sdfFactor)) {
            placeItem(pos, rot);
            itemPlaced = true;
            break;
          }
        }
      }
      if (!itemPlaced && (!useSubgrid || item.nextCellIdx >= totalCells)) {
        if (!item.noMoreFit) {
          LOGI("  retired " << item.name << ": no fit in "
                            << (useSubgrid ? totalCells : 1u)
                            << (useSubgrid ? " cells" : " full container search")
                            << " after " << searches << " searches this step\n");
        }
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
        LOGI("  every kind is out of cells to try\n");
        break;
      }
      packSuccess = true;
    }
    startNameIndex = (startNameIndex + 1) % numItems;
  }

  // exit reason, so a short step and an exhausted step are told apart.
  const char *why = "all kinds retired";
  if (outOfTime) {
    why = "step time limit";
  } else if (count >= step.count) {
    why = "target count reached";
  }
  report("step done");
  LOGI("  reason: " << why << ", " << count << " of " << step.count
                    << " rounds, " << placedCount << " placed, "
                    << (placedCount > 0
                            ? (stepClock.ElapsedMS() / double(placedCount))
                            : 0.0)
                    << " ms per placement, "
                    << (placedCount > 0
                            ? (double(searches) / double(placedCount))
                            : 0.0)
                    << " searches per placement\n");
}

// sample points on container surface, cast rays inward to gather depths

namespace {

bool RayAABB(const Vec3f &origin, const Vec3f &dir, const Box3f &box,
             float maxT, float &tmin) {
  float t0 = 0.0f;
  float t1 = maxT;
  for (int i = 0; i < 3; i++) {
    if (std::fabs(dir[i]) < 1e-8f) {
      if (origin[i] < box.vmin[i] || origin[i] > box.vmax[i]) {
        return false;
      }
    } else {
      float invD = 1.0f / dir[i];
      float tn = (box.vmin[i] - origin[i]) * invD;
      float tf = (box.vmax[i] - origin[i]) * invD;
      if (invD < 0.0f) {
        std::swap(tn, tf);
      }
      t0 = std::max(t0, tn);
      t1 = std::min(t1, tf);
      if (t0 > t1) {
        return false;
      }
    }
  }
  tmin = t0;
  return true;
}

Box3f WorldBox(const Box3f &localBox, const Matrix3f &rot, const Vec3f &pos) {
  Vec3f corners[8] = {
    localBox.vmin,
    Vec3f(localBox.vmax[0], localBox.vmin[1], localBox.vmin[2]),
    Vec3f(localBox.vmin[0], localBox.vmax[1], localBox.vmin[2]),
    Vec3f(localBox.vmax[0], localBox.vmax[1], localBox.vmin[2]),
    Vec3f(localBox.vmin[0], localBox.vmin[1], localBox.vmax[2]),
    Vec3f(localBox.vmax[0], localBox.vmin[1], localBox.vmax[2]),
    Vec3f(localBox.vmin[0], localBox.vmax[1], localBox.vmax[2]),
    localBox.vmax
  };
  Box3f wb;
  wb.vmin = rot * corners[0] + pos;
  wb.vmax = wb.vmin;
  for (int i = 1; i < 8; i++) {
    Vec3f w = rot * corners[i] + pos;
    for (int k = 0; k < 3; k++) {
      wb.vmin[k] = std::min(wb.vmin[k], w[k]);
      wb.vmax[k] = std::max(wb.vmax[k], w[k]);
    }
  }
  return wb;
}

struct InstanceAccel {
  Box3f worldBox;
  GridInstance gridInst;
};

std::vector<InstanceAccel> BuildInstanceAccel(PackingScene &scene) {
  std::vector<InstanceAccel> accel(scene.instances.size());
  for (size_t i = 0; i < scene.instances.size(); i++) {
    const InstanceInfo &inst = scene.instances[i];
    unsigned itemId = inst.itemId;
    auto it = scene.kindGrids.find(itemId);
    if (it == scene.kindGrids.end()) {
      auto g = std::make_shared<TrigGrid>();
      g->Build(scene.items[itemId].mesh, scene.gridDx);
      it = scene.kindGrids.emplace(itemId, g).first;
    }
    accel[i].gridInst = GridInstance::Local(it->second.get(), inst.tran.rotation,
                                             inst.tran.position, inst.tran.scale);
    accel[i].worldBox = WorldBox(scene.items[itemId].box, inst.tran.rotation,
                                  inst.tran.position);
  }
  return accel;
}

// Casts a world-space ray against one instance's TrigGrid. Returns world-
// space distance of nearest hit, or -1 if no hit within worldMaxT.
float RayInstanceHit(const GridInstance &gi, const Vec3f &worldOrigin,
                     const Vec3f &worldDir, float worldMaxT) {
  Vec3f localOrigin = gi.ToLocal(worldOrigin);
  Vec3f localDir = gi.rotInv * worldDir;
  localDir.normalize();
  float localMaxT = worldMaxT * gi.invScale;
  float localT = localMaxT;
  if (gi.grid->RayHit(localOrigin, localDir, localMaxT, localT)) {
    return localT * gi.scale;
  }
  return -1.0f;
}

void SaveDepthRaysObj(const std::string &filename,
                      const std::vector<Vec3f> &origins,
                      const std::vector<Vec3f> &ends) {
  std::ofstream out(filename);
  for (size_t i = 0; i < origins.size(); i++) {
    out << "v " << origins[i][0] << " " << origins[i][1] << " " << origins[i][2] << "\n";
    out << "v " << ends[i][0] << " " << ends[i][1] << " " << ends[i][2] << "\n";
    out << "l " << (2 * i + 1) << " " << (2 * i + 2) << "\n";
  }
}

}  // namespace

void ComputeSurfaceDepths(PackingScene &scene) {
  const float SAMPLE_EPS = 0.3f;
  Vec3f containerExtent = scene.container.box.vmax - scene.container.box.vmin;
  float maxDepth = std::min({containerExtent[0], containerExtent[1],
                             containerExtent[2]});

  float missedDepth = 0.1f;
  std::vector<SamplePoint> points;
  SamplePoints(scene.container.mesh, SAMPLE_EPS, points);

  // some parts can stick out of container  a little.
  float containerSlack = 0.5f;
  // Debug check: the melon container is centered near the origin, so every
  // surface normal should roughly point toward the origin. Count and report
  // any that don't.
  unsigned wrongDir = 0;
  unsigned checked = 0;
  unsigned shownBad = 0;
  for (size_t i = 0; i < points.size(); i++) {
    Vec3f toOrigin = -points[i].x;
    if (toOrigin.norm() < 1e-6f) {
      continue;
    }
    toOrigin.normalize();
    Vec3f n = points[i].n;
    if (n.norm() < 1e-6f) {
      continue;
    }
    n.normalize();
    checked++;
    float d = n.dot(toOrigin);
    if (d < 0.0f) {
      wrongDir++;
      if (shownBad < 5) {
        std::cout << "[surface depths] bad normal at point ("
                  << points[i].x[0] << " " << points[i].x[1] << " "
                  << points[i].x[2] << ") normal (" << n[0] << " " << n[1]
                  << " " << n[2] << ") dot_to_origin=" << d << "\n";
        shownBad++;
      }
    }
  }
  std::cout << "[surface depths] normal direction check: " << wrongDir << "/"
            << checked << " normals point away from origin\n";

  std::vector<InstanceAccel> accel = BuildInstanceAccel(scene);

  std::vector<Vec3f> origins(points.size());
  std::vector<Vec3f> ends(points.size());
  unsigned debugFlips = 0;
  unsigned hitCount = 0;
  for (size_t i = 0; i < points.size(); i++) {    
    Vec3f O = points[i].x;
    Vec3f dir = points[i].n;
    if (dir.norm() < 1e-6f) {
      ends[i] = points[i].x;
      continue;
    }
    dir.normalize();
    // Pick the normal direction that points toward the origin.
    // only works for debugging a centered melon, not a hand.
    Vec3f toOrigin = -points[i].x;
    if (toOrigin.norm() > 1e-6f) {
      toOrigin.normalize();
      if (dir.dot(toOrigin) < 0.0f) {
        dir = -dir;
        debugFlips++;
      }
    }

    // account for parts to stick out of container a little.
    O += -containerSlack * dir;
    Box3f rayBox;
    rayBox.vmin = O;
    rayBox.vmax = O + maxDepth * dir;
    for (int k = 0; k < 3; k++) {
      if (rayBox.vmin[k] > rayBox.vmax[k]) {
        std::swap(rayBox.vmin[k], rayBox.vmax[k]);
      }
    }

    std::vector<unsigned> candidates = scene.broadPhase.GetNearby(rayBox, 0.0f);

    float bestT = maxDepth;
    bool hit = false;
    for (unsigned instId : candidates) {
      float tmin;
      if (!RayAABB(O, dir, accel[instId].worldBox, bestT, tmin)) {
        continue;
      }
      float t = RayInstanceHit(accel[instId].gridInst, O, dir, bestT);
      if (t > 0.0f && t < bestT) {
        bestT = t;
        hit = true;
      }
    }
    origins[i] = O;
    ends[i] = O + (hit ? bestT : missedDepth) * dir;
    if (hit) {
      hitCount++;
    }
  }

  std::string depthFile = scene.outputFolder + "/surface_depths.obj";
  SaveDepthRaysObj(depthFile, origins, ends);
  LOGI("surface depths: " << hitCount << "/" << points.size()
                          << " rays hit an item, saved " << depthFile << "\n");
  LOGI("flips : " << debugFlips << "/" << points.size());
}

void PackScene(PackingScene &scene, const PackingPlan &plan, const PackingConfig &cfg) {
  PrepareBackground(scene, cfg);

  scene.packFile = scene.outputFolder + "/pack";
  scene.trajFile = scene.outputFolder + "/traj";
  scene.placed.resize(scene.items.size());

  if (cfg.resume) {
    LoadPack(scene, cfg.ResumePackPath());
  }
  ComputeSurfaceDepths(scene);
  for (size_t i = cfg.startStep; i < plan.steps.size(); i++) {
    LOGI("=== step " << i << " of " << (plan.steps.size() - 1) << " ===\n");
    Utils::Stopwatch clock;
    clock.Start();
    size_t before = scene.instances.size();
    PackStep(scene, plan.steps[i], cfg);
    LOGI("=== step " << i << " took " << (clock.ElapsedMS() / 1000.0) << " s, "
                     << (scene.instances.size() - before) << " placed, "
                     << scene.instances.size() << " instances total ===\n");
  }
}

void PackFruits(const PackingPlan &plan, const PackingConfig &cfgIn) {
  // the plan is only known here, so this is the first point at which
  // startStep can be checked against something real.
  PackingConfig cfg = cfgIn;
  cfg.ClampStartStep(plan.steps.size());
  PackingScene scene;
  if (!BuildScene(scene, cfg)) {
    return;
  }
  PackScene(scene, plan, cfg);
}
