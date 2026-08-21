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

  // The interval saves above only fire when the round counter crosses a
  // multiple of trajSaveInterval/packSaveInterval, so a step that places
  // items but never reaches that many rounds (e.g. a nearly full
  // container that retires every kind within a handful of rounds) never
  // saves anything. Do one unconditional save of the current state here
  // so a step's progress is never silently lost.
  if (placedCount > 0) {
    if (cfg.trajSaveInterval > 0) {
      scene.SaveTrajectories(scene.trajFile + "_final.txt");
    }
    if (cfg.packSaveInterval > 0) {
      scene.SaveInstances(scene.packFile + "_final.txt");
    }
  }
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

// Casts one inward ray per container surface sample point against all
// placed instances (broadphase + TrigGrid narrow phase). Records the
// origin (pulled back by containerSlack), the hit end point, the hit
// depth, and which instance (if any) was hit.
struct RayDepthResult {
  std::vector<Vec3f> origins;
  std::vector<Vec3f> ends;
  std::vector<float> depths;
  // -1 if the ray missed everything within maxDepth.
  std::vector<int> hitInstance;
  unsigned hitCount = 0;
};

RayDepthResult ComputeRayDepths(PackingScene &scene,
                                const std::vector<SamplePoint> &points,
                                const std::vector<InstanceAccel> &accel,
                                float maxDepth, float missedDepth,
                                float containerSlack) {
  RayDepthResult result;
  result.origins.resize(points.size());
  result.ends.resize(points.size());
  result.depths.resize(points.size(), 0.0f);
  result.hitInstance.assign(points.size(), -1);
  for (size_t i = 0; i < points.size(); i++) {
    Vec3f O = points[i].x;
    Vec3f dir = points[i].n;
    if (dir.norm() < 1e-6f) {
      result.origins[i] = points[i].x;
      result.ends[i] = points[i].x;
      continue;
    }
    dir.normalize();
    Vec3f toOrigin = -points[i].x;
    if (toOrigin.norm() > 1e-6f) {
      toOrigin.normalize();
      if (dir.dot(toOrigin) < 0.0f) {
        dir = -dir;
      }
    }

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
    int bestInst = -1;
    for (unsigned instId : candidates) {
      float tmin;
      if (!RayAABB(O, dir, accel[instId].worldBox, bestT, tmin)) {
        continue;
      }
      float t = RayInstanceHit(accel[instId].gridInst, O, dir, bestT);
      if (t > 0.0f && t < bestT) {
        bestT = t;
        hit = true;
        bestInst = int(instId);
      }
    }
    result.origins[i] = O;
    float depth = hit ? bestT : missedDepth;
    result.depths[i] = depth;
    result.ends[i] = O + depth * dir;
    result.hitInstance[i] = bestInst;
    if (hit) {
      result.hitCount++;
    }
  }
  return result;
}


// Uniform grid for point neighbors. Cell size matches the query radius
// so a 3x3x3 neighborhood covers all candidates.
struct PointGrid {
  float cellSize = 1.0f;
  Vec3f origin;
  Vec3u dims;
  Array3D<std::vector<unsigned>> cells;

  void Build(const std::vector<Vec3f> &points, float radius) {
    cellSize = radius;
    if (points.empty()) {
      dims = Vec3u(0, 0, 0);
      return;
    }
    Box3f box = ComputeBBox(points);
    origin = box.vmin;
    Vec3f extent = box.vmax - box.vmin;
    dims[0] = unsigned(std::floor(extent[0] / cellSize)) + 1;
    dims[1] = unsigned(std::floor(extent[1] / cellSize)) + 1;
    dims[2] = unsigned(std::floor(extent[2] / cellSize)) + 1;
    cells.Allocate(dims, {});
    for (size_t i = 0; i < points.size(); i++) {
      unsigned cx, cy, cz;
      PosToCell(points[i], cx, cy, cz);
      cells(cx, cy, cz).push_back(unsigned(i));
    }
  }

  void PosToCell(const Vec3f &p, unsigned &cx, unsigned &cy, unsigned &cz) const {
    cx = unsigned(std::floor((p[0] - origin[0]) / cellSize));
    cy = unsigned(std::floor((p[1] - origin[1]) / cellSize));
    cz = unsigned(std::floor((p[2] - origin[2]) / cellSize));
    cx = std::min(cx, dims[0] - 1);
    cy = std::min(cy, dims[1] - 1);
    cz = std::min(cz, dims[2] - 1);
  }

  std::vector<unsigned> Neighbors(const Vec3f &p, float radius) const {
    std::vector<unsigned> result;
    int cx, cy, cz;
    cx = int(std::floor((p[0] - origin[0]) / cellSize));
    cy = int(std::floor((p[1] - origin[1]) / cellSize));
    cz = int(std::floor((p[2] - origin[2]) / cellSize));
    int reach = int(std::ceil(radius / cellSize));
    float r2 = radius * radius;
    for (int dz = -reach; dz <= reach; dz++) {
      int gz = cz + dz;
      if (gz < 0 || gz >= int(dims[2])) continue;
      for (int dy = -reach; dy <= reach; dy++) {
        int gy = cy + dy;
        if (gy < 0 || gy >= int(dims[1])) continue;
        for (int dx = -reach; dx <= reach; dx++) {
          int gx = cx + dx;
          if (gx < 0 || gx >= int(dims[0])) continue;
          for (unsigned idx : cells(gx, gy, gz)) {
            result.push_back(idx);
          }
        }
      }
    }
    return result;
  }
};

// Extract rays whose depth exceeds the median depth of their neighbors
// by more than deepThreshold. Returns indices of deep rays.
//
// A raw median-vs-neighbors test misfires next to a smooth, big, convex
// surface (e.g. a kiwi) that already has small fruit seeded against part
// of it: samples right behind the small fruit are shallow (blocked by
// it), while samples just past its silhouette see straight through to
// the big surface and read a much larger, but perfectly normal, depth.
// That forms two separate populations in one 1 cm neighborhood, and the
// shallow one drags the median down enough to flag the far population as
// "deep" even though it is not an isolated pocket -- several neighbors
// share essentially the same depth, i.e. it is a broad patch of the big
// surface, not a narrow crevice. A genuine crevice bottom is a local
// outlier: few or no neighbors share its depth, since the pocket is
// narrow and most surrounding rays stop at the pocket's (shallower)
// walls. So a ray is only flagged when it clears the neighbor median by
// deepThreshold AND is not corroborated by at least minPatchNeighbors
// other rays within patchDepthTol of its own depth.
std::vector<unsigned> FindDeepRays(const std::vector<Vec3f> &origins,
                                   const std::vector<float> &depths,
                                   float neighborRadius,
                                   float deepThreshold,
                                   float patchDepthTol = 0.3f,
                                   unsigned minPatchNeighbors = 3) {
  PointGrid grid;
  grid.Build(origins, neighborRadius);
  std::vector<unsigned> deepRays;
  for (size_t i = 0; i < origins.size(); i++) {
    std::vector<unsigned> neighbors = grid.Neighbors(origins[i], neighborRadius);
    if (neighbors.size() < 3) {
      continue;
    }
    std::vector<float> neighborDepths;
    neighborDepths.reserve(neighbors.size());
    float r2 = neighborRadius * neighborRadius;
    unsigned patchNeighbors = 0;
    for (unsigned idx : neighbors) {
      if (idx == i) continue;
      if ((origins[idx] - origins[i]).norm2() > r2) continue;
      neighborDepths.push_back(depths[idx]);
      if (std::fabs(depths[idx] - depths[i]) <= patchDepthTol) {
        patchNeighbors++;
      }
    }
    if (neighborDepths.size() < 3) {
      continue;
    }
    if (patchNeighbors >= minPatchNeighbors) {
      // corroborated by a broad patch at roughly the same depth: this is
      // a smoothly varying surface, not an isolated crevice.
      continue;
    }
    std::sort(neighborDepths.begin(), neighborDepths.end());
    float median = neighborDepths[neighborDepths.size() / 2];
    if (depths[i] - median > deepThreshold) {
      deepRays.push_back(unsigned(i));
    }
  }
  return deepRays;
}

// Shoots one instance along each deep ray direction, starting at the ray
// origin (outside the container by containerSlack), and settles it with
// the rigid body solver. Cycles round robin through itemIndices so the
// seeded fruits are a mix of kinds, not just the smallest one. Skips a
// ray if it lands too close to an already-seeded position, to avoid
// stacking many fruits into one mouth of a crevice. Returns the number
// of instances placed.
unsigned SeedDeepCrevices(PackingScene &scene, const std::vector<Vec3f> &origins,
                          const std::vector<Vec3f> &ends,
                          const std::vector<unsigned> &itemIndices) {
  if (itemIndices.empty()) {
    return 0;
  }
  std::vector<Vec3f> seededPos;
  unsigned angleIndex = 0;
  unsigned itemCursor = 0;
  unsigned seeded = 0;
  for (size_t i = 0; i < origins.size(); i++) {
    const Vec3f &O = origins[i];
    unsigned itemIdx = itemIndices[itemCursor % itemIndices.size()];
    MeshInfo &item = scene.items[itemIdx];
    float exclusionDist = item.BoxDiagonal();
    bool tooClose = false;
    for (const Vec3f &p : seededPos) {
      if ((p - O).norm() < exclusionDist) {
        tooClose = true;
        break;
      }
    }
    if (tooClose) {
      continue;
    }
    Vec3f dir = ends[i] - O;
    if (dir.norm() < 1e-6f) {
      continue;
    }
    dir.normalize();

    RigidTransform tran;
    tran.position = O;
    Vec3f rot = scene.randAngles[angleIndex];
    angleIndex = (angleIndex + 1) % unsigned(scene.randAngles.size());
    tran.rotation = RotationMatrixRad(rot[0], rot[1], rot[2]);

    std::vector<RigidTransform> trajectory;
    RigidTransform settled = scene.Nudge(itemIdx, tran, dir, trajectory);
    unsigned instanceId = scene.Put(itemIdx, settled);
    scene.instances[instanceId].trajectory = trajectory;
    seededPos.push_back(settled.position);
    seeded++;
    itemCursor++;
  }
  LOGI("seeded " << seeded << "/" << origins.size() << " deep rays with "
                 << itemIndices.size() << " small fruit kinds round robin\n");
  return seeded;
}

}  // namespace

// Runs the raycasting pass and saves surface_depths.obj / deep_rays.obj.
// Returns the deep ray origins/ends via out params so a caller can decide
// whether to seed fruit into them. Does not place or settle anything, so
// it is safe to call without running the packing steps.
void ComputeAndSaveSurfaceDepths(PackingScene &scene,
                                 std::vector<Vec3f> &deepOrigins,
                                 std::vector<Vec3f> &deepEnds) {
  const float SAMPLE_EPS = 0.3f;
  Vec3f containerExtent = scene.container.box.vmax - scene.container.box.vmin;
  float maxDepth = std::min({containerExtent[0], containerExtent[1],
                             containerExtent[2]});

  float missedDepth = 0.1f;
  float containerSlack = 0.5f;

  std::vector<SamplePoint> points;
  SamplePoints(scene.container.mesh, SAMPLE_EPS, points);

  std::vector<InstanceAccel> accel = BuildInstanceAccel(scene);

  RayDepthResult res = ComputeRayDepths(scene, points, accel, maxDepth,
                                        missedDepth, containerSlack);

  std::string depthFile = scene.outputFolder + "/surface_depths.obj";
  SaveDepthRaysObj(depthFile, res.origins, res.ends);
  LOGI("surface depths: " << res.hitCount << "/" << points.size()
                          << " rays hit an item, saved " << depthFile << "\n");

  const float neighborRadius = 1.0f;
  const float deepThreshold = 0.5f;
  std::vector<unsigned> deepRays = FindDeepRays(res.origins, res.depths,
                                                neighborRadius, deepThreshold);
  deepOrigins.resize(deepRays.size());
  deepEnds.resize(deepRays.size());
  for (size_t i = 0; i < deepRays.size(); i++) {
    deepOrigins[i] = res.origins[deepRays[i]];
    deepEnds[i] = res.ends[deepRays[i]];
  }
  std::string deepFile = scene.outputFolder + "/deep_rays.obj";
  SaveDepthRaysObj(deepFile, deepOrigins, deepEnds);
  LOGI("deep rays: " << deepRays.size() << " rays exceed neighbor median by "
                     << deepThreshold << " cm, saved " << deepFile << "\n");
}

// Debug helper: finds the container-surface ray whose origin is closest
// to targetPos, then prints its depth alongside every neighbor ray
// within neighborRadius (same neighborhood FindDeepRays would use),
// including which instance/item each one hit and the angle between the
// two ray directions. Does not place or settle anything, does not save
// any file. Intended to be called directly from a small standalone
// harness (build scene, load resume pack, call this) without running
// PackScene/PackStep.
void DebugDeepRayNeighbors(PackingScene &scene, const Vec3f &targetPos) {
  const float SAMPLE_EPS = 0.3f;
  const float neighborRadius = 1.0f;
  const float deepThreshold = 0.5f;
  Vec3f containerExtent = scene.container.box.vmax - scene.container.box.vmin;
  float maxDepth = std::min({containerExtent[0], containerExtent[1],
                             containerExtent[2]});
  float missedDepth = 0.1f;
  float containerSlack = 0.5f;

  std::vector<SamplePoint> points;
  SamplePoints(scene.container.mesh, SAMPLE_EPS, points);
  std::vector<InstanceAccel> accel = BuildInstanceAccel(scene);
  RayDepthResult res = ComputeRayDepths(scene, points, accel, maxDepth,
                                        missedDepth, containerSlack);

  size_t bestIdx = 0;
  float bestDist = 1e30f;
  for (size_t i = 0; i < res.origins.size(); i++) {
    float d = (res.origins[i] - targetPos).norm2();
    if (d < bestDist) {
      bestDist = d;
      bestIdx = i;
    }
  }

  auto describeHit = [&](int instId) -> std::string {
    if (instId < 0) {
      return "MISS";
    }
    unsigned itemId = scene.instances[instId].itemId;
    return scene.items[itemId].name + " (inst " + std::to_string(instId) + ")";
  };

  Vec3f tO = res.origins[bestIdx];
  Vec3f tDir = res.ends[bestIdx] - tO;
  float tDepth = res.depths[bestIdx];
  tDir.normalize();
  std::cout << "\n=== DEBUG DEEP RAY near (" << targetPos[0] << " "
            << targetPos[1] << " " << targetPos[2] << ") ===\n";
  std::cout << "target: idx=" << bestIdx << " dist_to_query="
            << std::sqrt(bestDist) << "\n";
  std::cout << "  origin (" << tO[0] << " " << tO[1] << " " << tO[2] << ")\n";
  std::cout << "  dir (" << tDir[0] << " " << tDir[1] << " " << tDir[2] << ")\n";
  std::cout << "  depth=" << tDepth << " hit=" << describeHit(res.hitInstance[bestIdx])
            << "\n";

  PointGrid grid;
  grid.Build(res.origins, neighborRadius);
  std::vector<unsigned> neighbors = grid.Neighbors(tO, neighborRadius);
  float r2 = neighborRadius * neighborRadius;
  std::vector<float> neighborDepths;
  const float patchDepthTol = 0.3f;
  const unsigned minPatchNeighbors = 3;
  unsigned patchNeighbors = 0;
  std::cout << "  neighbors within " << neighborRadius << " cm:\n";
  for (unsigned idx : neighbors) {
    if (idx == bestIdx) continue;
    float d2 = (res.origins[idx] - tO).norm2();
    if (d2 > r2) continue;
    Vec3f nDir = res.ends[idx] - res.origins[idx];
    nDir.normalize();
    float cosAngle = std::clamp(tDir.dot(nDir), -1.0f, 1.0f);
    float angleDeg = std::acos(cosAngle) * 180.0f / 3.14159265f;
    neighborDepths.push_back(res.depths[idx]);
    bool samePatch = std::fabs(res.depths[idx] - tDepth) <= patchDepthTol;
    if (samePatch) {
      patchNeighbors++;
    }
    std::cout << "    idx=" << idx << " dist=" << std::sqrt(d2)
              << " depth=" << res.depths[idx]
              << " hit=" << describeHit(res.hitInstance[idx])
              << " dirAngle=" << angleDeg << " deg"
              << (samePatch ? " [same patch]" : "") << "\n";
  }
  std::cout << "  patch neighbors (within " << patchDepthTol << " cm of target depth): "
            << patchNeighbors << " (need < " << minPatchNeighbors
            << " to be eligible for the deep flag)\n";
  if (neighborDepths.size() < 3) {
    std::cout << "  fewer than 3 neighbors, FindDeepRays would skip this ray\n";
  } else {
    std::sort(neighborDepths.begin(), neighborDepths.end());
    float median = neighborDepths[neighborDepths.size() / 2];
    bool flaggedDeep = (patchNeighbors < minPatchNeighbors) &&
                       ((tDepth - median) > deepThreshold);
    std::cout << "  neighbor median depth=" << median << " margin="
              << (tDepth - median) << " threshold=" << deepThreshold
              << " flaggedDeep=" << flaggedDeep << "\n";
  }
  std::cout << "=== END DEBUG DEEP RAY ===\n\n";
}

void ComputeSurfaceDepths(PackingScene &scene,
                          const std::vector<std::string> &smallItemNames) {
  std::vector<Vec3f> deepOrigins;
  std::vector<Vec3f> deepEnds;
  ComputeAndSaveSurfaceDepths(scene, deepOrigins, deepEnds);

  std::vector<unsigned> smallItems;
  for (const std::string &name : smallItemNames) {
    auto it = scene.nameToIndex.find(name);
    if (it != scene.nameToIndex.end()) {
      smallItems.push_back(it->second);
    }
  }
  if (smallItems.empty()) {
    // fallback: no explicit small fruit list, use the single smallest item.
    std::vector<int> bySize = SortBySize(scene.items);
    if (!bySize.empty()) {
      smallItems.push_back(unsigned(bySize.back()));
    }
  }
  unsigned seeded = SeedDeepCrevices(scene, deepOrigins, deepEnds, smallItems);
  // SeedDeepCrevices has no interval-save logic of its own (it isn't part
  // of the PackStep round loop), so save once here if it placed anything,
  // otherwise those instances only exist in memory for the rest of the run.
  if (seeded > 0 && !scene.trajFile.empty() && !scene.packFile.empty()) {
    scene.SaveTrajectories(scene.trajFile + "_final.txt");
    scene.SaveInstances(scene.packFile + "_final.txt");
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
  ComputeSurfaceDepths(scene, plan.groups.empty() ? std::vector<std::string>() : plan.groups.back());
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
