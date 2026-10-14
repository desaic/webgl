#include "PackingDriver.h"

#include "AdapSDF.h"
#include "FastSweep.h"
#include "FloodOutside.h"
#include "GridUtils.h"
#include "Log.h"
#include "MarchingCubes.h"
#include "MeshInfo.h"
#include "MeshOps.h"
#include "PackingOps.h"
#include "PackShrinkWrap.h"
#include "Profiler.h"
#include "SphereCover.h"
#include "Stopwatch.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <execution>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <limits>
#include <map>
#include <numeric>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

// fruit-vs-fruit overlap tolerance for TryFillSpot/PackStep acceptance.
static const float kOccupiedThresh = 0.15f;
// container-containment tolerance, stricter and separate from the above.
static const float kContainerOutsideThresh = 0.05f;

static float ContainerOutsideFraction(const std::vector<SamplePoint> &samples,
                                      const Matrix3f &rot, const Vec3f &pos,
                                      const std::shared_ptr<AdapSDF> &sdf);
static float SignedOverlapFraction(PackingScene &scene, const Box3f &candidateLocalBox,
                                   const std::vector<SamplePoint> &candidateSamples,
                                   const Matrix3f &candRot, const Vec3f &candPos);
static float ExistingInsideCandidateFraction(PackingScene &scene, const Box3f &candidateLocalBox,
                                             const Matrix3f &rot, const Vec3f &pos,
                                             const std::shared_ptr<AdapSDF> &itemSdf);

// Returns the centroid of the k nearest points in pts to pos.
static Vec3f NearestKCenter(const std::vector<Vec3f> &pts, const Vec3f &pos, unsigned k) {
  if (pts.empty()) {
    return pos;
  }
  using Pair = std::pair<float, unsigned>;
  std::vector<Pair> heap;
  heap.reserve(k + 1);
  for (unsigned i = 0; i < (unsigned)pts.size(); i++) {
    float d2 = (pts[i] - pos).norm2();
    heap.push_back({d2, i});
    std::push_heap(heap.begin(), heap.end());
    if (heap.size() > k) {
      std::pop_heap(heap.begin(), heap.end());
      heap.pop_back();
    }
  }
  Vec3f center(0, 0, 0);
  for (const auto &p : heap) {
    center += pts[p.second];
  }
  center *= 1.0f / float(heap.size());
  return center;
}

// counts free (value 0) voxels within each subgrid cell of bg.
// free voxels of one subgrid cell.
static unsigned CountCellEmpty(const MeshConvo &bg, float cellSize, const Vec3u &numCells,
                               unsigned cellIdx) {
  const float dx = bg.dx;
  const Vec3u bgSize = bg.GridSize();
  const unsigned cx = cellIdx % numCells[0];
  const unsigned cy = (cellIdx / numCells[0]) % numCells[1];
  const unsigned cz = cellIdx / (numCells[0] * numCells[1]);
  const int x0 = int(cx * cellSize / dx);
  const int y0 = int(cy * cellSize / dx);
  const int z0 = int(cz * cellSize / dx);
  const int x1 = std::min(int((cx + 1) * cellSize / dx), (int)bgSize[0]);
  const int y1 = std::min(int((cy + 1) * cellSize / dx), (int)bgSize[1]);
  const int z1 = std::min(int((cz + 1) * cellSize / dx), (int)bgSize[2]);
  unsigned empty = 0;
  for (int z = z0; z < z1; z++)
    for (int y = y0; y < y1; y++)
      for (int x = x0; x < x1; x++)
        if (bg.vox((unsigned)x, (unsigned)y, (unsigned)z) == 0) empty++;
  return empty;
}

// full recount, cells in parallel.
static void UpdateCellEmptyCounts(SubgridBgCache &cache, const MeshConvo &bg,
                                  float cellSize, const Vec3u &numCells) {
  PROFILE_SCOPE("subgrid.empty_counts_full");
  const unsigned totalCells = numCells[0] * numCells[1] * numCells[2];
  cache.cellEmptyCounts.assign(totalCells, 0u);
  std::vector<unsigned> order(totalCells);
  std::iota(order.begin(), order.end(), 0u);
  std::for_each(std::execution::par, order.begin(), order.end(), [&](unsigned cellIdx) {
    cache.cellEmptyCounts[cellIdx] = CountCellEmpty(bg, cellSize, numCells, cellIdx);
  });
  cache.dirtyCells.clear();
  cache.emptyCacheVersion = bg.version;
  cache.emptyCacheNumCells = numCells;
}

// recount only the cells that a placement touched.
static void UpdateDirtyCellEmptyCounts(SubgridBgCache &cache, const MeshConvo &bg,
                                       float cellSize, const Vec3u &numCells) {
  PROFILE_SCOPE("subgrid.empty_counts_dirty");
  std::sort(cache.dirtyCells.begin(), cache.dirtyCells.end());
  cache.dirtyCells.erase(std::unique(cache.dirtyCells.begin(), cache.dirtyCells.end()),
                         cache.dirtyCells.end());
  std::for_each(std::execution::par, cache.dirtyCells.begin(), cache.dirtyCells.end(),
                [&](unsigned cellIdx) {
                  cache.cellEmptyCounts[cellIdx] = CountCellEmpty(bg, cellSize, numCells, cellIdx);
                });
  cache.dirtyCells.clear();
  cache.emptyCacheVersion = bg.version;
}

// remembers the cells overlapped by a freshly placed fruit (world box, padded
// by two voxels for the erosion and union margins).
static void MarkDirtyCells(SubgridBgCache &cache, const PackingScene &scene, const Box3f &box) {
  const MeshConvo &bg = scene.bg;
  const Vec3u numCells = scene.numSubgridCells;
  const Vec3f origin = bg.GetOrigin();
  const float dx = bg.dx;
  int lo[3], hi[3];
  for (int a = 0; a < 3; ++a) {
    const float vmin = (box.vmin[a] - origin[a]) / dx - 2.0f;
    const float vmax = (box.vmax[a] - origin[a]) / dx + 2.0f;
    lo[a] = std::max(0, int(std::floor(vmin * dx / scene.subgridCellSize)));
    hi[a] = std::min(int(numCells[a]) - 1, int(std::floor(vmax * dx / scene.subgridCellSize)));
  }
  for (int z = lo[2]; z <= hi[2]; ++z)
    for (int y = lo[1]; y <= hi[1]; ++y)
      for (int x = lo[0]; x <= hi[0]; ++x)
        cache.dirtyCells.push_back(unsigned(x) + numCells[0] * (unsigned(y) + numCells[1] * unsigned(z)));
}

// counts filled voxels in axis-aligned voxelization of item at resolution dx.
static unsigned CountItemVoxels(const MeshInfo &item, float dx) {
  Box3f bbox = item.box;
  VoxConf conf;
  conf.origin = ToArray(AlignOriginToGrid(bbox.vmin, dx));
  conf.unit = {dx, dx, dx};
  conf.gridSize = ComputeGridSize(bbox, dx, 1);
  Array3D8u vox;
  vox.Allocate(conf.gridSize, 0);
  VoxelizeMesh(item.mesh, vox, conf);
  FloodOutside8u(vox, 1, 2);
  unsigned count = 0;
  for (uint8_t v : vox.GetData()) if (v != 0) count++;
  return count;
}

static void DownsampleBg2x(const MeshConvo &fine, MeshConvo &coarse) {
  Vec3u fs = fine.GridSize();
  Vec3u cs((fs[0]+1)/2, (fs[1]+1)/2, (fs[2]+1)/2);
  coarse.box = fine.box;
  coarse.dx = fine.dx * 2.0f;
  coarse.vox.Allocate(cs, 0);
  for (unsigned z = 0; z < cs[2]; z++)
    for (unsigned y = 0; y < cs[1]; y++)
      for (unsigned x = 0; x < cs[0]; x++) {
        uint8_t v = 0;
        for (unsigned dz = 0; dz < 2 && !v; dz++)
        for (unsigned dy = 0; dy < 2 && !v; dy++)
        for (unsigned dx2 = 0; dx2 < 2 && !v; dx2++) {
          unsigned fx=2*x+dx2, fy=2*y+dy, fz=2*z+dz;
          if (fx<fs[0] && fy<fs[1] && fz<fs[2])
            v = fine.vox(fx, fy, fz);
        }
        coarse.vox(x, y, z) = v;
      }
}

static const float kLargeMedLargeThresh = 14.0f;

static bool IsNearSpherical(const MeshInfo &item) {
  Vec3f ext = item.box.vmax - item.box.vmin;
  float minE = std::min({ext[0], ext[1], ext[2]});
  float maxE = std::max({ext[0], ext[1], ext[2]});
  return maxE > 0.0f && (maxE - minE) / maxE < 0.25f;
}

static const Vec3f kSphereRotations[3] = {
  {0.0f, 0.0f, 0.0f},
  {1.5707963f, 0.0f, 0.0f},
  {0.0f, 1.5707963f, 0.0f}
};

bool BuildScene(PackingScene &scene, const PackingConfig &cfg) {
  std::string meshDir = cfg.MeshDir();
  if (!fs::exists(meshDir)) {
    std::cout << "item directory missing " << meshDir << "\n";
    return false;
  }
  Utils::Stopwatch loadClock;
  loadClock.Start();
  {
    PROFILE_SCOPE("init.load_items");
    scene.items = LoadAllMeshInfo(meshDir);
  }
  LOGI("init: loaded items " << loadClock.ElapsedMS() / 1000.0 << " s\n");
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
  std::error_code outputError;
  fs::create_directories(scene.outputFolder, outputError);
  if (outputError) {
    LOGI("could not create output folder " << scene.outputFolder << ": "
         << outputError.message() << "\n");
    return false;
  }

  if (!cfg.exclusionMeshFile.empty()) {
    MeshInfo excl;
    if (LoadMeshInfo(excl, fs::path(cfg.ExclusionMeshPath())) == 0) {
      excl.name = "_exclusion";
      scene.exclusionItemIndex = scene.items.size();
      scene.items.push_back(std::move(excl));
      LOGI("exclusion mesh loaded: " << cfg.ExclusionMeshPath() << "\n");
    } else {
      LOGI("exclusion mesh not found: " << cfg.ExclusionMeshPath() << "\n");
    }
  }

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
  if (cfg.oldWayOverlap) {
    // second grid without the container wall, for the overlap heuristic.
    scene.itemsBg = MeshConvo();
    scene.itemsBg.box = scene.bg.box;
    scene.itemsBg.dx = scene.bg.dx;
    scene.itemsBg.vox.Allocate(scene.bg.vox.GetSize(), 0);
  }
}

void PackStep(PackingScene &scene, const PackingStep &step, const PackingConfig &cfg,
              const std::vector<Vec3f> &surfacePoints, PlacementTimer *timer) {
  // per-step breakdown (findspot.*, findspot_subgrid.*, nudge.* -- see the
  // PROFILE_SCOPE calls in PackingOps.cpp/PackingScene.cpp), printed at the
  // end of this function. Reset here so each step's report is its own
  // cost, not cumulative since process start; the real pipeline never
  // called Profiler::Report before, only the benchmark harness did, so
  // this was accumulating silently on every run without ever being shown.
  Profiler::Reset();
  // round counter (one round tries every kind in the step once), NOT a
  // placement counter -- step.count is a placement cap, checked against
  // placedCount below. A round can place up to numItems instances (one per
  // kind), so bounding the loop on round count alone (as this used to do)
  // let a small step.count place far more than intended once there were
  // enough kinds in the step to place several per round.
  unsigned round = 0;
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

  unsigned startItem = cfg.startItem;
  if (startItem >= numItems) {
    LOGI("  startItem " << startItem << " past the last of " << numItems
                        << " kinds in this step, clamped to "
                        << (numItems - 1) << "\n");
    startItem = numItems - 1;
  }

  unsigned long searches = 0;
  unsigned placedCount = 0;
  std::map<std::string, double> itemSearchMs;
  std::unordered_map<unsigned, unsigned> itemVoxCounts;
  // shared across every item/cell tried in this step -- its own key
  // (cell, crop bounds, scene.bg's version) decides reuse, so interleaving
  // between kinds/cells cannot make it serve a stale background crop.
  SubgridBgCache subgridCache;
  SubgridBgCache coarseSubgridCache;
  MeshConvo coarseBg;
  unsigned coarseBgVersion = ~0u;
  // non-subgrid ("large" tier) equivalent of subgridCache, against the
  // WHOLE container grid instead of a cropped cell -- see the FindSpot
  // call below.
  unsigned bgFftVersion = 0;
  bool bgFftValid = false;
  unsigned coarseBgFftVersion = ~0u;
  bool coarseBgFftValid = false;
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

  for (; placedCount < step.count; round++) {
    if (cfg.maxSecondsPerStep > 0.0 && stepClock.ElapsedMS() / 1000.0 > cfg.maxSecondsPerStep) {
      outOfTime = true;
      break;
    }
    bool packSuccess = false;
    for (unsigned i = startItem; i < numItems; i++) {
      if (placedCount >= step.count) {
        break;
      }
      // checked per kind, not just once per round -- a single kind can walk
      // its entire subgrid (up to numCells*maxTrialCount searches) before
      // giving up, which on its own can take far longer than the round-level
      // check above allows for.
      if (cfg.maxSecondsPerStep > 0.0 && stepClock.ElapsedMS() / 1000.0 > cfg.maxSecondsPerStep) {
        outOfTime = true;
        break;
      }
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

      auto placeItem = [&](const Vec3f &p, const Vec3f &r) -> bool {
        RigidTransform tran;
        tran.position = p;
        tran.rotation = RotationMatrixRad(r[0], r[1], r[2]);
        double settleStartMs = stepClock.ElapsedMS();
        std::vector<RigidTransform> trajectory;
        RigidTransform newTran;
        if (!surfacePoints.empty()) {
          Vec3f target = NearestKCenter(surfacePoints, p, 5);
          newTran = scene.NudgeToTarget(itemIndex, tran, target, trajectory);
        } else {
          Vec3f pushDir = step.nudgeInward
                              ? scene.ForceDirection(itemIndex, Vec3f(0, 0, 0), 0.0f, -1.0f, tran)
                              : scene.ForceDirection(itemIndex, step.force, step.biasW, sdfFactor, tran);
          newTran = scene.Nudge(itemIndex, tran, pushDir, step.forceW, trajectory);
        }
        double settleMs = stepClock.ElapsedMS() - settleStartMs;
        if (cfg.oldWayMedium && newTran.position[0] >= cfg.sphereMaxX) {
          LOGI("  rejected " << item.name << ": settled outside the x < " << cfg.sphereMaxX << " region\n");
          return false;
        }
        // no fruit may be wasted inside the exclusion mesh: reject any settle
        // whose surface samples penetrate it. (its SDF is negative inside.)
        if (cfg.oldWayMedium && scene.exclusionItemIndex != ~0u && !item.samples.empty()) {
          MeshInfo &ex = scene.items[scene.exclusionItemIndex];
          if (!ex.sdf) ex.ComputeSDFCached();
          if (ex.sdf) {
            float worst = 0.0f;
            for (const SamplePoint &sp : item.samples) {
              const Vec3f w = newTran.rotation * sp.x + newTran.position;
              worst = std::min(worst, ex.sdf->GetCoarseDist(w));
            }
            if (worst < -0.1f) {
              LOGI("  rejected " << item.name << ": " << -worst << " cm inside the exclusion mesh\n");
              return false;
            }
          }
        }
        if (timer) {
          timer->Record(item.name, settleMs);
        }
        // the settle can drag an item into an already-placed neighbor;
        // verify with the same overlap checks TryFillSpot's gate uses.
        if (!item.samples.empty()) {
          float outsideFracAfter =
              ContainerOutsideFraction(item.samples, newTran.rotation, newTran.position, scene.sdf);
          float occFracAfter =
              SignedOverlapFraction(scene, item.box, item.samples, newTran.rotation, newTran.position);
          float engulfFrac =
              ExistingInsideCandidateFraction(scene, item.box, newTran.rotation, newTran.position, item.sdf);
          if (outsideFracAfter > kContainerOutsideThresh || occFracAfter > kOccupiedThresh ||
              engulfFrac > kOccupiedThresh) {
            LOGI("  rejected " << item.name << " settle at (" << newTran.position[0] << " "
                               << newTran.position[1] << " " << newTran.position[2]
                               << "): outside=" << outsideFracAfter << " occ=" << occFracAfter
                               << " engulf=" << engulfFrac << ", retrying\n");
            return false;
          }
        }
        packSuccess = true;
        unsigned instanceId = scene.Put(itemIndex, newTran);
        MarkDirtyCells(subgridCache, scene,
                       ComputeBBox(MakeTransformedMesh(scene.items[itemIndex].mesh, newTran).v));
        scene.instances[instanceId].trajectory = trajectory;
        placedCount++;
        Vec3f moved = newTran.position - p;
        LOGI("  placed " << scene.items[itemIndex].name << " instance "
                         << instanceId << " cell " << item.nextCellIdx
                         << " at " << newTran.position[0] << " "
                         << newTran.position[1] << " " << newTran.position[2]
                         << ", settled " << moved.norm() << " cm in "
                         << settleMs << " ms, "
                         << placedCount << " this step\n");
        if (cfg.trajSaveInterval > 0 && placedCount % cfg.trajSaveInterval == 0 && placedCount > 0) {
          std::string trajFile = scene.trajFile
                                 + std::to_string(int(placedCount / cfg.trajSaveInterval) % 10)
                                 + ".txt";
          scene.SaveTrajectories(trajFile);
        }
        if (cfg.packSaveInterval > 0 && placedCount % cfg.packSaveInterval == 0 && placedCount > 0) {
          std::string packFile = scene.packFile
                                 + std::to_string(int(placedCount / cfg.packSaveInterval) % 10)
                                 + ".txt";
          scene.SaveInstances(packFile);
        }
        return true;
      };

      bool itemPlaced = false;
      double itemSearchStart = stepClock.ElapsedMS();

      // opt 1: per-item voxel count, computed once at the item's first trial
      if (useSubgrid && itemVoxCounts.find(itemIndex) == itemVoxCounts.end()) {
        itemVoxCounts[itemIndex] = CountItemVoxels(item, scene.dx);
      }
      unsigned itemVox = useSubgrid ? itemVoxCounts[itemIndex] : 0u;

      // opt 1: refresh empty count cache whenever bg changes
      if (useSubgrid &&
          (subgridCache.emptyCacheVersion != scene.bg.version ||
           subgridCache.emptyCacheNumCells != scene.numSubgridCells)) {
        const unsigned totalCellsNow =
            scene.numSubgridCells[0] * scene.numSubgridCells[1] * scene.numSubgridCells[2];
        const bool canUpdateDirty = subgridCache.emptyCacheVersion != ~0u &&
                                    subgridCache.emptyCacheNumCells == scene.numSubgridCells &&
                                    subgridCache.cellEmptyCounts.size() == totalCellsNow &&
                                    !subgridCache.dirtyCells.empty();
        if (canUpdateDirty) {
          UpdateDirtyCellEmptyCounts(subgridCache, scene.bg, scene.subgridCellSize,
                                     scene.numSubgridCells);
        } else {
          UpdateCellEmptyCounts(subgridCache, scene.bg, scene.subgridCellSize,
                                scene.numSubgridCells);
        }
      }

      // opt 2: spherical items need only 3 axis-aligned rotation trials
      bool spherical = IsNearSpherical(item);
      unsigned trialLimit = spherical ? 3u : MAX_TRIAL_COUNT;

      // opt 3+4: large/medium-large items use 2x coarser bg + 1-voxel erosion
      float itemDiag = (item.box.vmax - item.box.vmin).norm();
      bool isLargeMedLarge = itemDiag >= kLargeMedLargeThresh;
      if (isLargeMedLarge && coarseBgVersion != scene.bg.version) {
        DownsampleBg2x(scene.bg, coarseBg);
        coarseBg.version = scene.bg.version;
        coarseBgVersion = scene.bg.version;
        coarseBgFftValid = false;
      }
      // step-level erosion takes precedence over the item-size heuristic
      unsigned erode = (step.erodeVoxels > 0) ? step.erodeVoxels
                       : (isLargeMedLarge ? 1u : 0u);

      if (useSubgrid) {
        while (!itemPlaced && item.nextCellIdx < totalCells) {
          unsigned cellIdx = item.nextCellIdx;
          // region test run: skip cells that start at or past the x limit.
          if (cfg.oldWayMedium) {
            const float cellMinX = scene.bg.box.vmin[0] +
                                   float(cellIdx % scene.numSubgridCells[0]) * scene.subgridCellSize;
            if (cellMinX >= cfg.sphereMaxX) {
              item.nextCellIdx++;
              continue;
            }
          }
          // opt 1: skip cell if it has fewer free voxels than the item needs
          if (subgridCache.emptyCacheVersion == scene.bg.version &&
              cellIdx < subgridCache.cellEmptyCounts.size() &&
              subgridCache.cellEmptyCounts[cellIdx] < itemVox) {
            item.nextCellIdx++;
            continue;
          }
          bool cellSuccess = false;
          for (unsigned trial = 0; trial < trialLimit; trial++) {
            Vec3f pos;
            Vec3f rot;
            if (spherical) {
              rot = kSphereRotations[trial];
            } else {
              rot = scene.randAngles[angleIndex];
              angleIndex++;
              if (angleIndex >= scene.randAngles.size()) {
                angleIndex = 0;
              }
            }
            searches++;
            TrigMesh rotatedMesh = item.mesh;
            TransformVerts(item.mesh.v, rotatedMesh.v,
                           RotationMatrixRad(rot[0], rot[1], rot[2]));
            MeshConvo &subBg = isLargeMedLarge ? coarseBg : scene.bg;
            SubgridBgCache *subCache = isLargeMedLarge ? &coarseSubgridCache : &subgridCache;
            if (FindSpotSubgrid(subBg, rotatedMesh, pos, scene.sdf,
                                sdfFactor, scene.subgridCellSize,
                                cellIdx, scene.numSubgridCells, subCache, erode,
                                step.overlapHeuristic,
                                (step.overlapHeuristic && !isLargeMedLarge &&
                                 !scene.itemsBg.vox.GetData().empty())
                                    ? &scene.itemsBg : nullptr)) {
              if (placeItem(pos, rot)) {
                itemPlaced = true;
                cellSuccess = true;
                break;
              }
            }
          }
          if (cellSuccess) {
            // stay on this cell -- it can hold more than one item.
            break;
          }
          double cellMs = stepClock.ElapsedMS() - itemSearchStart;
          LOGD("  no fit " << item.name << " cell " << cellIdx
                           << " " << cellMs << " ms\n");
          item.nextCellIdx++;
        }
      } else {
        // scene.bg's own FFT, valid until the next successful placement
        // (bg.version bumps on Union/UnionReversed, see MeshConvo.h) --
        // FindSpot otherwise recomputes it from scratch on every single
        // trial, unconditionally, even though it is identical across all
        // of them whenever nothing has been placed since (measured: 11%
        // of one step's total wall time, entirely redundant work).
        MeshConvo &activeBg = isLargeMedLarge ? coarseBg : scene.bg;
        if (isLargeMedLarge) {
          if (!coarseBgFftValid || coarseBgFftVersion != scene.bg.version) {
            Vec3u gs = coarseBg.GridSize();
            coarseBg.FFT(PadSizes(gs, 8));
            coarseBgFftVersion = scene.bg.version;
            coarseBgFftValid = true;
          }
        } else {
          if (!bgFftValid || bgFftVersion != scene.bg.version) {
            Vec3u bgGridSize = scene.bg.GridSize();
            scene.bg.FFT(PadSizes(bgGridSize, 8));
            bgFftVersion = scene.bg.version;
            bgFftValid = true;
          }
        }
        for (unsigned trial = 0; trial < trialLimit; trial++) {
          Vec3f pos;
          Vec3f rot;
          if (spherical) {
            rot = kSphereRotations[trial];
          } else {
            rot = scene.randAngles[angleIndex];
            angleIndex++;
            if (angleIndex >= scene.randAngles.size()) {
              angleIndex = 0;
            }
          }
          searches++;
          TrigMesh rotatedMesh = item.mesh;
          TransformVerts(item.mesh.v, rotatedMesh.v,
                         RotationMatrixRad(rot[0], rot[1], rot[2]));
          if (FindSpot(activeBg, rotatedMesh, pos, scene.sdf, sdfFactor, /*bgFftReady=*/true, erode,
                      step.overlapHeuristic)) {
            if (placeItem(pos, rot)) {
              itemPlaced = true;
              break;
            }
          }
        }
      }
      double roundMs = stepClock.ElapsedMS() - itemSearchStart;
      itemSearchMs[item.name] += roundMs;
      if (!itemPlaced && !useSubgrid) {
        LOGI("  no fit " << item.name << " full search " << roundMs << " ms\n");
      }
      if (!itemPlaced && (!useSubgrid || item.nextCellIdx >= totalCells)) {
        if (!item.noMoreFit) {
          LOGI("  retired " << item.name << ": no fit in "
                            << (useSubgrid ? totalCells : 1u)
                            << (useSubgrid ? " cells" : " full container search")
                            << " after " << searches << " searches this step"
                            << ", search time " << itemSearchMs[item.name] << " ms\n");
        }
        item.noMoreFit = true;
      }
    }
    if (outOfTime) {
      break;
    }
    if (!packSuccess) {
      bool anySubgridRemaining = false;
      unsigned tCells = scene.numSubgridCells[0] * scene.numSubgridCells[1]
                        * scene.numSubgridCells[2];
      for (unsigned nameIndex = 0; nameIndex < numItems; nameIndex++) {
        unsigned itemIdx = scene.GetItemIndex(step.names[nameIndex]);
        MeshInfo &it = scene.items[itemIdx];
        if (!it.noMoreFit && tCells > 0 && it.nextCellIdx < tCells) {
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

  const char *why = "all kinds retired";
  if (outOfTime) {
    why = "step time limit";
  } else if (placedCount >= step.count) {
    why = "target count reached";
  }
  report("step done");
  LOGI("  reason: " << why << ", " << round << " rounds, " << placedCount
                    << " of " << step.count << " placed, "
                    << (placedCount > 0
                            ? (stepClock.ElapsedMS() / double(placedCount))
                            : 0.0)
                    << " ms per placement, "
                    << (placedCount > 0
                            ? (double(searches) / double(placedCount))
                            : 0.0)
                    << " searches per placement\n");
  Profiler::Report(std::cout, "step cost breakdown", stepClock.ElapsedMS());

  if (placedCount > 0) {
    if (cfg.trajSaveInterval > 0) {
      scene.SaveTrajectories(scene.trajFile + "_final.txt");
    }
    if (cfg.packSaveInterval > 0) {
      scene.SaveInstances(scene.packFile + "_final.txt");
    }
  }
}

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

// one inward ray per container surface sample point, against all placed
// instances.
struct RayDepthResult {
  std::vector<Vec3f> origins;
  std::vector<Vec3f> ends;
  std::vector<float> depths;
  std::vector<int> hitInstance;  // -1 if missed within maxDepth.
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


// uniform grid for point-neighbor queries; cell size == query radius.
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

// rays whose depth exceeds their neighbor median by deepThreshold.
std::vector<unsigned> FindDeepRays(const std::vector<Vec3f> &origins,
                                   const std::vector<float> &depths,
                                   float neighborRadius,
                                   float deepThreshold,
                                   float patchDepthTol = 0.3f
                                   ) {
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

    std::sort(neighborDepths.begin(), neighborDepths.end());
    float median = neighborDepths[neighborDepths.size() / 2];
    if (depths[i] - median > deepThreshold) {
      deepRays.push_back(unsigned(i));
    }
  }
  return deepRays;
}

// fraction of transformed sample points landing in occupied bg voxels.
float OccupiedFraction(const PackingScene &scene,
                       const std::vector<SamplePoint> &samples,
                       const Matrix3f &rot,
                       const Vec3f &pos) {
  if (samples.empty()) {
    return 0.0f;
  }
  const MeshConvo &bg = scene.bg;
  Vec3f origin = bg.GetOrigin();
  float invDx = 1.0f / bg.dx;
  Vec3u gridSize = bg.vox.GetSize();
  unsigned occupied = 0;
  for (const auto &sp : samples) {
    Vec3f w = rot * sp.x + pos;
    int ix = int((w[0] - origin[0]) * invDx);
    int iy = int((w[1] - origin[1]) * invDx);
    int iz = int((w[2] - origin[2]) * invDx);
    if (ix < 0 || iy < 0 || iz < 0 ||
        (unsigned)ix >= gridSize[0] ||
        (unsigned)iy >= gridSize[1] ||
        (unsigned)iz >= gridSize[2]) {
      occupied++;
      continue;
    }
    if (bg.vox((unsigned)ix, (unsigned)iy, (unsigned)iz) != 0) {
      occupied++;
    }
  }
  return float(occupied) / float(samples.size());
}

unsigned SeedDeepCrevices(PackingScene &scene, const std::vector<Vec3f> &origins,
                          const std::vector<Vec3f> &ends,
                          const std::vector<unsigned> &itemIndices,
                          const std::vector<Vec3f> &surfacePoints) {
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
    tran.position = O + dir * (0.5f + 0.5f * item.BoxDiagonal());
    Vec3f rot = scene.randAngles[angleIndex];
    angleIndex = (angleIndex + 1) % unsigned(scene.randAngles.size());
    tran.rotation = RotationMatrixRad(rot[0], rot[1], rot[2]);

    const std::vector<SamplePoint> &samples = item.samples;
    if (!samples.empty()) {
      float occFrac = OccupiedFraction(scene, samples, tran.rotation, tran.position);
      if (occFrac > 0.5f) {
        std::cout << "skip crevice ray " << i << " occFrac=" << occFrac << "\n";
        continue;
      }
    }

    std::vector<RigidTransform> trajectory;
    Vec3f target = NearestKCenter(surfacePoints, O, 5);
    NudgeOutcome outcome;
    RigidTransform settled = scene.NudgeToTarget(itemIdx, tran, target, trajectory, &outcome);
    if (!samples.empty()) {
      float occFracAfter = OccupiedFraction(scene, samples, settled.rotation, settled.position);
      if (occFracAfter > 0.5f) {
        std::cout << "skip crevice ray " << i << " outcome="
                  << (outcome == NudgeOutcome::Arrived ? "arrived" :
                      outcome == NudgeOutcome::Jammed ? "jammed" : "out_of_steps")
                  << " post-settle occFrac=" << occFracAfter << "\n";
        itemCursor++;
        continue;
      }
    }
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

bool ClaimGrid::IsClaimed(const Vec3f &pos) const {
  for (const auto &c : claims) {
    if ((pos - c.first).norm() < c.second) {
      return true;
    }
  }
  return false;
}

void ClaimGrid::Mark(const Vec3f &pos, float radius) {
  claims.push_back({pos, radius});
}

// fine-grained OccupiedFraction, against the EnvelopeField instead of
// scene.bg.vox.
static float EnvelopeOccupiedFraction(const std::vector<SamplePoint> &samples,
                                      const Matrix3f &rot, const Vec3f &pos,
                                      const EnvelopeField &envelope) {
  if (samples.empty() || envelope.dist.GetSize()[0] == 0) {
    return 0.0f;
  }
  unsigned occupied = 0;
  for (const auto &sp : samples) {
    Vec3f w = rot * sp.x + pos;
    float d = SampleDistField(envelope.dist, envelope.origin, envelope.voxelSize,
                              envelope.distUnit, w);
    if (d <= 0.0f) {
      occupied++;
    }
  }
  return float(occupied) / float(samples.size());
}

// fraction of item samples landing outside the container (scene.sdf > 0).
static float ContainerOutsideFraction(const std::vector<SamplePoint> &samples,
                                      const Matrix3f &rot, const Vec3f &pos,
                                      const std::shared_ptr<AdapSDF> &sdf) {
  if (samples.empty() || !sdf) {
    return 0.0f;
  }
  unsigned outside = 0;
  for (const auto &sp : samples) {
    Vec3f w = rot * sp.x + pos;
    if (sdf->GetCoarseDist(w) > 0.0f) {
      outside++;
    }
  }
  return float(outside) / float(samples.size());
}

// fraction of the candidate's own samples landing inside a nearby
// existing instance's signed per-kind sdf. Unlike EnvelopeOccupiedFraction
// (unsigned distance to the nearest fruit surface, merged across
// instances), this can't be fooled by a sample deep inside a neighbor
// reading as "far from any surface" == "free".
static float SignedOverlapFraction(PackingScene &scene, const Box3f &candidateLocalBox,
                                   const std::vector<SamplePoint> &candidateSamples,
                                   const Matrix3f &candRot, const Vec3f &candPos) {
  if (candidateSamples.empty()) {
    return 0.0f;
  }
  Box3f worldBox = WorldBox(candidateLocalBox, candRot, candPos);
  std::vector<unsigned> nearby = scene.broadPhase.GetNearby(worldBox, 0.0f);
  if (nearby.empty()) {
    return 0.0f;
  }
  unsigned occupied = 0;
  for (const auto &sp : candidateSamples) {
    Vec3f w = candRot * sp.x + candPos;
    for (unsigned instId : nearby) {
      const InstanceInfo &inst = scene.instances[instId];
      MeshInfo &existingItem = scene.items[inst.itemId];
      if (!existingItem.sdf) {
        continue;
      }
      Vec3f localPt = inst.tran.rotation.transposed() * (w - inst.tran.position);
      if (existingItem.sdf->GetCoarseDist(localPt) < 0.0f) {
        occupied++;
        break;
      }
    }
  }
  return float(occupied) / float(candidateSamples.size());
}

// the reverse of SignedOverlapFraction: worst, over nearby existing
// instances, of how much of THAT instance's own samples land inside the
// candidate's sdf -- catches a small neighbor engulfed by a big candidate,
// which SignedOverlapFraction alone would miss.
static float ExistingInsideCandidateFraction(PackingScene &scene, const Box3f &candidateLocalBox,
                                              const Matrix3f &rot, const Vec3f &pos,
                                              const std::shared_ptr<AdapSDF> &itemSdf) {
  if (!itemSdf) {
    return 0.0f;
  }
  Box3f candidateBox = WorldBox(candidateLocalBox, rot, pos);
  std::vector<unsigned> nearby = scene.broadPhase.GetNearby(candidateBox, 0.0f);
  if (nearby.empty()) {
    return 0.0f;
  }
  Matrix3f rotInv = rot.transposed();
  float worst = 0.0f;
  for (unsigned instId : nearby) {
    const InstanceInfo &inst = scene.instances[instId];
    const std::vector<SamplePoint> &existingSamples = scene.items[inst.itemId].samples;
    if (existingSamples.empty()) {
      continue;
    }
    unsigned inside = 0;
    for (const auto &sp : existingSamples) {
      Vec3f w = inst.tran.rotation * sp.x + inst.tran.position;
      Vec3f localPt = rotInv * (w - pos);
      if (itemSdf->GetCoarseDist(localPt) < 0.0f) {
        inside++;
      }
    }
    worst = std::max(worst, float(inside) / float(existingSamples.size()));
  }
  return worst;
}

// walks outward from target along outwardDir until SignedOverlapFraction/
// ExistingInsideCandidateFraction read clear, capped at maxDist. Falls
// back to the least-overlapping point tried if nothing is fully clear.
static Vec3f FindClearSpawnPos(PackingScene &scene, const Box3f &candidateLocalBox,
                               const std::vector<SamplePoint> &samples, const Vec3f &target,
                               const Vec3f &outwardDir, const Matrix3f &rot,
                               const std::shared_ptr<AdapSDF> &itemSdf, float maxDist) {
  if (samples.empty() || outwardDir.norm2() < 1e-12f) {
    return target;
  }
  const float STEP = 0.5f;
  const float kClearThresh = 0.02f;
  auto occAt = [&](const Vec3f &pos) {
    return std::max(SignedOverlapFraction(scene, candidateLocalBox, samples, rot, pos),
                    ExistingInsideCandidateFraction(scene, candidateLocalBox, rot, pos, itemSdf));
  };
  Vec3f dir = outwardDir.normalizedCopy();
  Vec3f bestPos = target;
  float bestOcc = occAt(target);
  for (float d = STEP; d <= maxDist; d += STEP) {
    Vec3f cand = target + dir * d;
    float occ = occAt(cand);
    if (occ < bestOcc) {
      bestOcc = occ;
      bestPos = cand;
    }
    if (occ <= kClearThresh) {
      return cand;
    }
  }
  return bestPos;
}

int TryFillSpot(PackingScene &scene, const Vec3f &target, float localWidth,
                const std::vector<unsigned> &kindsBySize, float clearance,
                const EnvelopeField &envelope, ClaimGrid &claims,
                PlacementTimer *timer, bool sizeMustFitLocalWidth) {
  static unsigned angleCursor = 0;

  // nudge 1cm past target, into the interior, so the spring has a nonzero
  // pull from step 0 (a settle starting exactly at its own target has no
  // initial force). outwardDir also gives FindClearSpawnPos an escape
  // direction (Phase 2 only), pointing the other way.
  Vec3f nudgeTarget = target;
  Vec3f outwardDir(0.0f);
  if (scene.sdf) {
    Vec3f grad = scene.sdf->GetCoarseGrad(target);
    float gradNorm = grad.norm();
    if (gradNorm > 1e-6f) {
      outwardDir = grad * (1.0f / gradNorm);
      nudgeTarget = target - outwardDir;
    }
  }

  for (unsigned itemIdx : kindsBySize) {
    MeshInfo &item = scene.items[itemIdx];
    if (sizeMustFitLocalWidth && item.BoxDiagonal() + clearance > localWidth) {
      continue;
    }
    RigidTransform tran;
    tran.position = target;
    Vec3f rot = scene.randAngles[angleCursor % scene.randAngles.size()];
    angleCursor++;
    tran.rotation = RotationMatrixRad(rot[0], rot[1], rot[2]);

    // item.samples/sdf populate as a side effect of NudgeToTarget's first
    // call for this kind, so this block is only live from the second use on.
    if (!item.samples.empty()) {
      if (!sizeMustFitLocalWidth) {
        float maxSearchDist = 10.0f * item.BoxDiagonal();
        tran.position = FindClearSpawnPos(scene, item.box, item.samples, target, outwardDir,
                                          tran.rotation, item.sdf, maxSearchDist);
        LOGI("    FindClearSpawnPos " << item.name << " outwardDir=(" << outwardDir[0] << ","
                                       << outwardDir[1] << "," << outwardDir[2]
                                       << ") target=(" << target[0] << "," << target[1] << ","
                                       << target[2] << ") spawn=(" << tran.position[0] << ","
                                       << tran.position[1] << "," << tran.position[2] << ")\n");
      }
      float occFracBefore = EnvelopeOccupiedFraction(item.samples, tran.rotation, tran.position, envelope);
      float outsideFracBefore = ContainerOutsideFraction(item.samples, tran.rotation, tran.position, scene.sdf);
      if (occFracBefore > kOccupiedThresh || outsideFracBefore > kContainerOutsideThresh) {
        LOGI("    TryFillSpot " << item.name << " pre-reject occBefore=" << occFracBefore
                                 << " outsideBefore=" << outsideFracBefore << "\n");
        continue;
      }
      // reject hopeless starting poses before paying for a full settle.
      if (!sizeMustFitLocalWidth) {
        const float kPreOverlapThresh = 0.6f;
        float preOverlap = SignedOverlapFraction(scene, item.box, item.samples, tran.rotation, tran.position);
        float preEngulf = ExistingInsideCandidateFraction(scene, item.box, tran.rotation,
                                                           tran.position, item.sdf);
        if (preOverlap > kPreOverlapThresh || preEngulf > kPreOverlapThresh) {
          LOGI("    TryFillSpot " << item.name << " pre-reject preOverlap=" << preOverlap
                                   << " preEngulf=" << preEngulf << "\n");
          continue;
        }
      }
    }

    std::vector<RigidTransform> trajectory;
    NudgeOutcome outcome;
    Utils::Stopwatch settleClock;
    settleClock.Start();
    RigidTransform settled = scene.NudgeToTarget(itemIdx, tran, nudgeTarget, trajectory, &outcome);
    if (timer) {
      timer->Record(item.name, settleClock.ElapsedMS());
    }
    if (!item.samples.empty()) {
      float outsideFracAfter =
          ContainerOutsideFraction(item.samples, settled.rotation, settled.position, scene.sdf);
      if (outsideFracAfter > kContainerOutsideThresh) {
        LOGI("    TryFillSpot " << item.name << " post-reject outsideAfter=" << outsideFracAfter << "\n");
        continue;
      }
      float occFracAfter =
          SignedOverlapFraction(scene, item.box, item.samples, settled.rotation, settled.position);
      if (occFracAfter > kOccupiedThresh) {
        LOGI("    TryFillSpot " << item.name << " post-reject occAfter=" << occFracAfter << "\n");
        continue;
      }
      float engulfFrac =
          ExistingInsideCandidateFraction(scene, item.box, settled.rotation, settled.position, item.sdf);
      if (engulfFrac > kOccupiedThresh) {
        LOGI("    TryFillSpot " << item.name << " post-reject engulf=" << engulfFrac << "\n");
        continue;
      }
    }
    unsigned id = scene.Put(itemIdx, settled);
    scene.instances[id].trajectory = trajectory;
    claims.Mark(settled.position, item.BoxDiagonal());
    return int(id);
  }
  return -1;
}

void PackFillVoids(PackingScene &scene, const PackingConfig &cfg, PlacementTimer *timer) {
  std::vector<unsigned> kindsBySize(scene.items.size());
  for (unsigned i = 0; i < scene.items.size(); i++) {
    kindsBySize[i] = i;
  }
  std::sort(kindsBySize.begin(), kindsBySize.end(), [&](unsigned a, unsigned b) {
    return scene.items[a].BoxDiagonal() > scene.items[b].BoxDiagonal();
  });

  float threshold = cfg.shrinkwrapVoxelSize * 1.1f;
  for (unsigned round = 0; round < cfg.fillVoidMaxRounds; round++) {
    size_t before = scene.instances.size();
    Utils::Stopwatch clock;
    clock.Start();

    VoidField vf = ComputeVoidField(scene, cfg.shrinkwrapRadius, cfg.shrinkwrapVoxelSize);
    if (vf.dist.GetSize()[0] == 0) {
      LOGI("fill voids round " << round << ": void field is empty (no instances), stopping\n");
      break;
    }
    VoidSpotResult result = ExtractVoidSpots(vf, threshold, cfg.shrinkwrapOpenRadiusVoxels);
    EnvelopeField envelope = ComputeEnvelopeField(scene, cfg.shrinkwrapVoxelSize);

    ClaimGrid claims;
    unsigned filled = 0, claimed = 0, noFit = 0;
    for (const VoidSpot &s : result.spots) {
      if (claims.IsClaimed(s.pos)) {
        claimed++;
        continue;
      }
      float localWidth = 2.0f * s.radius;
      int id = TryFillSpot(scene, s.pos, localWidth, kindsBySize, cfg.fillVoidClearance,
                           envelope, claims, timer);
      if (id >= 0) {
        filled++;
      } else {
        noFit++;
      }
    }
    LOGI("fill voids round " << round << ": " << result.spots.size() << " spots, "
                             << filled << " placed, " << claimed << " claimed, "
                             << noFit << " no fit/settled, " << scene.instances.size()
                             << " instances total, " << (clock.ElapsedMS() / 1000.0)
                             << " s\n");
    if (scene.instances.size() - before < cfg.fillVoidMinPlaced) {
      LOGI("fill voids: round " << round << " placed fewer than "
                                << cfg.fillVoidMinPlaced << ", stopping\n");
      break;
    }
  }
}

static bool MaskAt(const Array3D8u &mask, const Vec3f &origin, float voxelSize,
                   const Vec3f &worldPos) {
  Vec3f local = (worldPos - origin) * (1.0f / voxelSize);
  int ix = int(std::floor(local[0]));
  int iy = int(std::floor(local[1]));
  int iz = int(std::floor(local[2]));
  Vec3u size = mask.GetSize();
  if (ix < 0 || iy < 0 || iz < 0 || (unsigned)ix >= size[0] || (unsigned)iy >= size[1] ||
      (unsigned)iz >= size[2]) {
    return false;
  }
  return mask((unsigned)ix, (unsigned)iy, (unsigned)iz) != 0;
}

struct CurrentVoid {
  VoidField vf;
  VoidSpotResult spots;
  bool IsOpen(const Vec3f &p) const {
    return MaskAt(spots.cleanedMask, vf.origin, vf.voxelSize, p);
  }
};

// Phase 2's own field: ComputeShrinkwrapField, not ComputeVoidField (that
// one is volumetric interior, used by ComputePackQuality/PackFillVoids).
// No morphological open here (openRadiusVoxels=0, a no-op) -- it was wide
// enough to keep a single fruit's own surface dimple as a "spot," which is
// not a crevice at all (nothing to bridge, no second fruit involved). The
// skin band itself can stay a tight 1 voxel now that nothing needs to
// survive erosion.
static CurrentVoid ComputeCurrentVoid(PackingScene &scene, const CreviceFieldParams &fp) {
  CurrentVoid cur;
  cur.vf = ComputeShrinkwrapField(scene, fp.shrinkwrapRadius, fp.shrinkwrapVoxelSize,
                                  fp.shrinkwrapOpenRadiusVoxels, 1);
  if (cur.vf.dist.GetSize()[0] > 0) {
    cur.spots = ExtractVoidSpots(cur.vf, fp.shrinkwrapVoxelSize * 1.1f, 0);
  }
  return cur;
}

static float OpenFraction(const std::vector<Vec3f> &voxels, const CurrentVoid &cur) {
  if (voxels.empty()) {
    return 0.0f;
  }
  unsigned open = 0;
  for (const Vec3f &p : voxels) {
    if (cur.IsOpen(p)) {
      open++;
    }
  }
  return float(open) / float(voxels.size());
}

CreviceBaseline ComputeCreviceBaseline(PackingScene &scene, const PackingConfig &cfg,
                                       const CreviceFieldParams &fp, size_t numInstances) {
  CreviceBaseline base;
  numInstances = std::min(numInstances, scene.instances.size());
  base.numBaseInstances = numInstances;
  // evaluate the field on just the prefix by temporarily dropping the tail.
  std::vector<InstanceInfo> tail(scene.instances.begin() + numInstances, scene.instances.end());
  scene.instances.erase(scene.instances.begin() + numInstances, scene.instances.end());
  CurrentVoid cur = ComputeCurrentVoid(scene, fp);
  scene.instances.insert(scene.instances.end(), tail.begin(), tail.end());
  if (cur.vf.dist.GetSize()[0] == 0) {
    return base;
  }
  base.origin = cur.vf.origin;
  base.voxelSize = cur.vf.voxelSize;
  base.allMask = cur.spots.cleanedMask;

  // assign voxels using every spot (not just in-range ones), so a >3cm void
  // keeps its own voxels instead of leaking into a neighboring small spot.
  const std::vector<VoidSpot> &all = cur.spots.spots;
  std::vector<int> inRangeIndex(all.size(), -1);
  for (size_t i = 0; i < all.size(); i++) {
    float width = 2.0f * all[i].radius;
    if (width >= cfg.creviceMinWidth && width <= cfg.creviceMaxWidth) {
      inRangeIndex[i] = int(base.spots.size());
      base.spots.push_back(all[i]);
    }
  }
  base.spotVoxels.resize(base.spots.size());
  if (all.empty()) {
    return base;
  }
  Vec3u size = base.allMask.GetSize();
  float h = base.voxelSize;
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        if (base.allMask(x, y, z) == 0) {
          continue;
        }
        Vec3f p = base.origin + Vec3f((x + 0.5f) * h, (y + 0.5f) * h, (z + 0.5f) * h);
        size_t best = 0;
        float bestD = 1e30f;
        for (size_t i = 0; i < all.size(); i++) {
          float d = (p - all[i].pos).norm() - all[i].radius;
          if (d < bestD) {
            bestD = d;
            best = i;
          }
        }
        if (inRangeIndex[best] >= 0) {
          base.spotVoxels[inRangeIndex[best]].push_back(p);
        }
      }
    }
  }
  return base;
}

namespace {

struct MediumPatchPoint {
  Vec3f pos;
  Vec3f normal;
  float clearance;
  unsigned radius;
  bool covered = false;
  unsigned owner = ~0u;
};

bool MediumPointCovered(const MediumPatchPoint &point, const MeshInfo &item,
                        const RigidTransform &pose) {
  if (!item.sdf || (point.pos - pose.position).norm2() >
                       0.6f * 0.6f * item.BoxDiagonal() * item.BoxDiagonal()) return false;
  const Vec3f local = pose.rotation.transposed() * (point.pos - pose.position);
  return item.sdf->GetCoarseDist(local) <= 0.2f;
}

float MediumSupportGap(PackingScene &scene, const MeshInfo &item,
                       const RigidTransform &pose) {
  const Box3f box = WorldBox(item.box, pose.rotation, pose.position);
  const auto nearby = scene.broadPhase.GetNearby(box, 1.0f);
  float bestGap = std::numeric_limits<float>::max();
  for (unsigned id : nearby) {
    const auto &inst = scene.instances[id];
    if (inst.isExclusion) continue;
    const auto &existing = scene.items[inst.itemId];
    if (!existing.sdf) continue;
    const size_t stride = std::max<size_t>(1, item.mesh.v.size() / 2048);
    for (size_t vi = 0; vi < item.mesh.v.size(); vi += stride) {
      Vec3f vertex = item.mesh.v[vi];
      Vec3f w = pose.rotation * vertex + pose.position;
      Vec3f local = inst.tran.rotation.transposed() * (w - inst.tran.position);
      bestGap = std::min(bestGap, std::fabs(existing.sdf->GetCoarseDist(local)));
      if (bestGap <= 0.30f) return bestGap;
    }
  }
  return bestGap;
}

bool MediumOverlapsTooDeeply(PackingScene &scene, const MeshInfo &item,
                             const RigidTransform &pose) {
  const Box3f box = WorldBox(item.box, pose.rotation, pose.position);
  const auto nearby = scene.broadPhase.GetNearby(box, 0.2f);
  const size_t stride = std::max<size_t>(1, item.mesh.v.size() / 2048);
  for (unsigned id : nearby) {
    const auto &inst = scene.instances[id];
    const auto &existing = scene.items[inst.itemId];
    if (!existing.sdf) continue;
    for (size_t vi = 0; vi < item.mesh.v.size(); vi += stride) {
      Vec3f vertex = item.mesh.v[vi];
      Vec3f w = pose.rotation * vertex + pose.position;
      Vec3f local = inst.tran.rotation.transposed() * (w - inst.tran.position);
      // 0.2cm allowed physical overlap plus 0.15cm SDF/sample error.
      if (existing.sdf->GetCoarseDist(local) < -0.35f) return true;
    }
  }
  return false;
}

// Gradient of the nearest existing fruit's signed distance field.
// A goal shifted along this direction is more likely to settle in contact
// than a goal on the shrinkwrap skin alone.
bool MediumNearestSupport(PackingScene &scene, const Vec3f &p,
                          float &distance, Vec3f &away) {
  Box3f query;
  query.vmin = query.vmax = p;
  distance = std::numeric_limits<float>::max();
  for (unsigned id : scene.broadPhase.GetNearby(query, 18.0f)) {
    const auto &inst = scene.instances[id];
    const auto &support = scene.items[inst.itemId];
    if (inst.isExclusion || !support.sdf) continue;
    Vec3f local = inst.tran.rotation.transposed() * (p - inst.tran.position);
    float d = support.sdf->GetCoarseDist(local);
    if (d < 0.0f || d >= distance) continue;
    const float h = 0.5f;
    Vec3f g(support.sdf->GetCoarseDist(local + Vec3f(h, 0, 0)) -
            support.sdf->GetCoarseDist(local - Vec3f(h, 0, 0)),
            support.sdf->GetCoarseDist(local + Vec3f(0, h, 0)) -
            support.sdf->GetCoarseDist(local - Vec3f(0, h, 0)),
            support.sdf->GetCoarseDist(local + Vec3f(0, 0, h)) -
            support.sdf->GetCoarseDist(local - Vec3f(0, 0, h)));
    if (g.norm2() < 1e-6f) continue;
    g.normalize();
    away = inst.tran.rotation * g;
    distance = d;
  }
  return distance < std::numeric_limits<float>::max();
}

bool MediumOutsideExclusion(PackingScene &scene, const MeshInfo &item,
                            const RigidTransform &pose) {
  if (scene.exclusionItemIndex == ~0u) return true;
  const auto &excl = scene.items[scene.exclusionItemIndex];
  if (!excl.sdf) return false; // never silently discard the hard obstacle
  for (const auto &sp : item.samples) {
    Vec3f w = pose.rotation * sp.x + pose.position;
    if (excl.sdf->GetCoarseDist(w) < 0.0f) return false;
  }
  return true;
}

unsigned MediumCoverageGain(const std::vector<MediumPatchPoint> &points,
                             const MeshInfo &item, const RigidTransform &pose) {
  unsigned gain = 0;
  for (const auto &p : points) {
    if (!p.covered && MediumPointCovered(p, item, pose)) gain++;
  }
  return gain;
}

struct MediumGraphResult {
  std::vector<unsigned> order;
  unsigned components = 0;
  unsigned fruitEdges = 0;
  unsigned openSampleEdges = 0;
  unsigned openComponents = 0;
  unsigned largestOpenComponent = 0;
};

// The crevice skin is the routing graph; medium fruits are seeds/owners on
// that graph. Its edges stay on the same wrap and cannot jump across free
// space as straight center-to-center fruit edges can.
MediumGraphResult BuildMediumPatchGraph(const std::vector<MediumPatchPoint> &points,
                                      const Vec3f &origin, float cellSize,
                                      const PackingScene &scene,
                                      const std::string &graphFile) {
  MediumGraphResult result;
  const unsigned n = unsigned(points.size());
  result.order.resize(n);
  std::iota(result.order.begin(), result.order.end(), 0u);
  auto coord = [&](const Vec3f &p, unsigned axis) {
    return int(std::floor((p[axis] - origin[axis]) / cellSize));
  };
  auto key = [](int x, int y, int z, unsigned radius) {
    return (uint64_t(radius == 12) << 60) |
           (uint64_t(x) << 40) | (uint64_t(y) << 20) | uint64_t(z);
  };
  std::unordered_map<uint64_t, unsigned> cells;
  cells.reserve(n * 2);
  for (unsigned i = 0; i < n; ++i)
    cells.emplace(key(coord(points[i].pos, 0), coord(points[i].pos, 1),
                      coord(points[i].pos, 2), points[i].radius), i);
  std::vector<std::vector<unsigned>> links(n);
  for (unsigned i = 0; i < n; ++i) {
    const auto &p = points[i];
    int x = coord(p.pos, 0), y = coord(p.pos, 1), z = coord(p.pos, 2);
    for (int dz = -1; dz <= 1; ++dz)
      for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
          if (x + dx < 0 || y + dy < 0 || z + dz < 0) continue;
          auto it = cells.find(key(x + dx, y + dy, z + dz, p.radius));
          if (it == cells.end() || it->second <= i) continue;
          unsigned j = it->second;
          Vec3f delta = points[j].pos - p.pos;
          float length = delta.norm();
          if (length > 5.5f || length < 1e-5f ||
              p.normal.dot(points[j].normal) < 0.25f ||
              std::fabs(p.normal.dot(delta)) > 0.6f * length ||
              std::fabs(points[j].normal.dot(delta)) > 0.6f * length) continue;
          links[i].push_back(j);
          links[j].push_back(i);
        }
  }
  std::vector<unsigned> component(n, ~0u), componentSize;
  std::deque<unsigned> queue;
  for (unsigned i = 0; i < n; ++i) {
    if (component[i] != ~0u) continue;
    unsigned cid = result.components++;
    unsigned size = 0;
    component[i] = cid;
    queue.push_back(i);
    while (!queue.empty()) {
      unsigned u = queue.front(); queue.pop_front(); ++size;
      for (unsigned v : links[u]) if (component[v] == ~0u) {
        component[v] = cid;
        queue.push_back(v);
      }
    }
    componentSize.push_back(size);
  }
  std::vector<int> distance(n, -1);
  std::vector<unsigned> owner(n, ~0u);
  for (unsigned i = 0; i < n; ++i) if (points[i].covered) {
    distance[i] = 0;
    owner[i] = points[i].owner;
    queue.push_back(i);
  }
  std::unordered_set<uint64_t> fruitEdges;
  while (!queue.empty()) {
    unsigned u = queue.front(); queue.pop_front();
    for (unsigned v : links[u]) {
      if (distance[v] < 0) {
        distance[v] = distance[u] + 1;
        owner[v] = owner[u];
        queue.push_back(v);
      } else if (owner[u] != ~0u && owner[v] != ~0u && owner[u] != owner[v]) {
        unsigned a = std::min(owner[u], owner[v]);
        unsigned b = std::max(owner[u], owner[v]);
        fruitEdges.insert((uint64_t(a) << 32) | b);
      }
    }
  }
  std::stable_sort(result.order.begin(), result.order.end(), [&](unsigned a, unsigned b) {
    // Midpoints of long uncovered graph paths go first. Unseeded patches
    // get a seed before short gaps; isolated single samples go last.
    int da = distance[a] < 0 ? (componentSize[component[a]] > 1 ? 1000 : 0) : distance[a];
    int db = distance[b] < 0 ? (componentSize[component[b]] > 1 ? 1000 : 0) : distance[b];
    if (da != db) return da > db;
    if (componentSize[component[a]] != componentSize[component[b]])
      return componentSize[component[a]] > componentSize[component[b]];
    return points[a].clearance > points[b].clearance;
  });
  std::unordered_map<unsigned, unsigned> vertex;
  std::ofstream out(graphFile);
  for (unsigned id : owner) if (id != ~0u && !vertex.count(id)) {
    vertex[id] = unsigned(vertex.size()) + 1;
    const Vec3f &p = scene.instances[id].tran.position;
    out << "v " << p[0] << ' ' << p[1] << ' ' << p[2] << '\n';
  }
  for (uint64_t edge : fruitEdges) {
    unsigned a = unsigned(edge >> 32), b = unsigned(edge);
    out << "l " << vertex[a] << ' ' << vertex[b] << '\n';
  }
  result.fruitEdges = unsigned(fruitEdges.size());
  std::vector<Vec3f> openEdgeMidpoints;
  for (unsigned i = 0; i < n; ++i) for (unsigned j : links[i]) {
    if (j <= i) continue;
    bool covered = points[i].covered && points[j].covered;
    if (covered && points[i].owner != points[j].owner) {
      MediumPatchPoint mid = points[i];
      mid.pos = (points[i].pos + points[j].pos) * 0.5f;
      covered = false;
      for (unsigned id : {points[i].owner, points[j].owner}) {
        if (id != ~0u && MediumPointCovered(mid, scene.items[scene.instances[id].itemId],
                                             scene.instances[id].tran)) {
          covered = true;
          break;
        }
      }
    }
    if (!covered) {
      ++result.openSampleEdges;
      openEdgeMidpoints.push_back((points[i].pos + points[j].pos) * 0.5f);
    }
  }
  if (!graphFile.empty())
    SaveVec3fObj(graphFile + "_open_edges.obj", openEdgeMidpoints);
  std::vector<bool> seen(n, false);
  for (unsigned i = 0; i < n; ++i) {
    if (points[i].covered || seen[i]) continue;
    unsigned openSize = 0;
    seen[i] = true;
    queue.push_back(i);
    while (!queue.empty()) {
      unsigned u = queue.front(); queue.pop_front(); ++openSize;
      for (unsigned v : links[u]) if (!points[v].covered && !seen[v]) {
        seen[v] = true;
        queue.push_back(v);
      }
    }
    ++result.openComponents;
    result.largestOpenComponent = std::max(result.largestOpenComponent, openSize);
  }
  return result;
}

} // namespace

void PackMediumPatches(PackingScene &scene, const PackingStep &step,
                       const PackingConfig &cfg, PlacementTimer *timer) {
  Utils::Stopwatch clock;
  clock.Start();
  const double maxSeconds = cfg.maxSecondsPerStep > 0 ?
      std::min(3600.0, cfg.maxSecondsPerStep) : 3600.0;
  const unsigned maxAttempts = 25000;
  std::vector<unsigned> kinds;
  for (const auto &name : step.names) {
    auto it = scene.nameToIndex.find(name);
    if (it != scene.nameToIndex.end()) kinds.push_back(it->second);
  }
  if (kinds.empty()) return;
  for (unsigned kind : kinds) scene.EnsureItemSamples(kind);
  float maxDiag = 0.0f;
  for (unsigned kind : kinds) maxDiag = std::max(maxDiag, scene.items[kind].BoxDiagonal());
  std::shared_ptr<AdapSDF> innerSdf;
  if (!scene.containerInner.mesh.v.empty()) {
    LOGI("medium patches: computing inner-container target mask\n");
    innerSdf = ComputeSDF(0.002f, 2.0f, scene.containerInner.mesh);
  }

  std::vector<MediumPatchPoint> points;
  std::unordered_set<uint64_t> sampled;
  const float sampleDx = 3.0f;
  unsigned filteredInner = 0, filteredExclusion = 0, filteredDeep = 0;
  Vec3f origin(0.0f, 0.0f, 0.0f);
  const std::string targetsPath = cfg.MediumTargetsPath();
  if (!targetsPath.empty()) {
    std::vector<Vec3f> loaded;
    if (!LoadObjVertices(targetsPath, loaded) || loaded.empty()) {
      LOGI("medium patches: failed to load targets from " << targetsPath << '\n');
      return;
    }
    origin = loaded[0];
    for (const auto &q : loaded)
      for (int c = 0; c < 3; ++c) origin[c] = std::min(origin[c], q[c]);
    for (const auto &q : loaded) {
      // Only positions are stored. Derive the free-space normal and the
      // void clearance from the nearest support fruit.
      float supportDist = 0.0f;
      Vec3f away(0.0f, 0.0f, 1.0f);
      if (!MediumNearestSupport(scene, q, supportDist, away)) supportDist = 0.0f;
      points.push_back({q, away, std::max(supportDist, 1.0f), 6u});
    }
    LOGI("medium patches: loaded " << points.size() << " targets from " << targetsPath << '\n');
    std::stable_sort(points.begin(), points.end(), [](const auto &a, const auto &b) {
      return a.clearance > b.clearance;
    });
  } else {
    // Snapshot only the fixed >14cm support tier for target generation. The
    // full instance vector and broad phase are restored before any placement.
    // Existing medium-small fruits may already be present in a resumed pack;
    // they count as coverage, never as generators of new baseline targets.
    std::vector<InstanceInfo> original;
    original.swap(scene.instances);
    for (const auto &inst : original) {
      if (!inst.isExclusion && scene.items[inst.itemId].BoxDiagonal() > 14.0f)
        scene.instances.push_back(inst);
    }
    VoidField large;
    large.voxelSize = step.shrinkwrapVoxelSize;
    Array3D<short> raw;
    try {
      ComputeShrinkWrapDistField(scene, 12.0f, step.shrinkwrapVoxelSize,
                                 large.dist, large.origin, large.distUnit, &raw);
    } catch (...) {
      scene.instances.swap(original);
      throw;
    }
    scene.instances.swap(original);
    if (raw.GetData().empty()) {
      LOGI("medium patches: no fixed support fruit\n");
      return;
    }
    Array3D<short> closed6 = CloseShrinkwrapField(raw, step.shrinkwrapVoxelSize,
                                                   step.shrinkwrapRadius, large.distUnit);
    const Vec3f origin = large.origin;
    const float dx = step.shrinkwrapVoxelSize, distUnit = large.distUnit;

    for (unsigned scale : {6u, 12u}) {
      const Array3D<short> &closed = scale == 6 ? closed6 : large.dist;
      TrigMesh hull = ComputeShrinkWrapMesh(closed, origin, dx, distUnit);
      for (size_t ti = 0; ti < hull.GetNumTrigs(); ++ti) {
        const Triangle tri = hull.GetTriangleVerts(ti);
        const Vec3f p = (tri[0] + tri[1] + tri[2]) * (1.0f / 3.0f);
        float gap = SampleDistField(raw, origin, dx, distUnit, p);
        if (gap <= 1.1f * dx || gap > 0.75f * maxDiag) continue;
        const float containerDist = scene.sdf->GetCoarseDist(p);
        if (containerDist > 0.0f) continue;
        // Only the outer shell matters for this pass. The inner mesh is a
        // target mask, not a collision boundary for the fruit itself.
        if (containerDist < -16.0f) { filteredDeep++; continue; }
        if (innerSdf && innerSdf->GetCoarseDist(p) <= 1.0f) {
          filteredInner++; continue;
        }
        if (scene.exclusionItemIndex != ~0u) {
          const auto &excl = scene.items[scene.exclusionItemIndex];
          if (excl.sdf && excl.sdf->GetCoarseDist(p) <= 2.5f) {
            filteredExclusion++; continue;
          }
        }
        if (scale == 12 && SampleDistField(closed6, origin, dx, distUnit, p) <= dx)
          continue;
        Vec3f n = (tri[1] - tri[0]).cross(tri[2] - tri[0]);
        if (n.norm2() < 1e-8f) continue;
        n.normalize();
        if (SampleDistField(closed, origin, dx, distUnit, p + 2.0f * n) <
            SampleDistField(closed, origin, dx, distUnit, p - 2.0f * n)) n *= -1.0f;
        Vec3f g = (p - origin) * (1.0f / sampleDx);
        const uint64_t x = uint64_t(std::max(0, int(std::floor(g[0]))));
        const uint64_t y = uint64_t(std::max(0, int(std::floor(g[1]))));
        const uint64_t z = uint64_t(std::max(0, int(std::floor(g[2]))));
        const uint64_t key = (uint64_t(scale == 12) << 48) | (x << 32) | (y << 16) | z;
        if (!sampled.insert(key).second) continue;
        points.push_back({p, n, gap, scale});
      }
    }
    raw = Array3D<short>();
    closed6 = Array3D<short>();
    large.dist = Array3D<short>();
    std::stable_sort(points.begin(), points.end(), [](const auto &a, const auto &b) {
      if (a.radius != b.radius) return a.radius < b.radius;
      return a.clearance > b.clearance;
    });
  }

  std::unordered_set<unsigned> mediumKinds(kinds.begin(), kinds.end());
  unsigned initiallyCovered = 0;
  for (auto &p : points) {
    Box3f query;
    query.vmin = query.vmax = p.pos;
    for (unsigned id : scene.broadPhase.GetNearby(query, maxDiag)) {
      const auto &inst = scene.instances[id];
      if (mediumKinds.count(inst.itemId) &&
          MediumPointCovered(p, scene.items[inst.itemId], inst.tran)) {
        p.covered = true;
        p.owner = id;
        initiallyCovered++;
        break;
      }
    }
  }
  std::vector<Vec3f> baselinePts;
  baselinePts.reserve(points.size());
  for (const auto &p : points) baselinePts.push_back(p.pos);
  SaveVec3fObj(scene.outputFolder + "/medium_patch_targets.obj", baselinePts);
  {
    std::ofstream targetCsv(scene.outputFolder + "/medium_patch_targets.csv");
    targetCsv << "x,y,z,nx,ny,nz,clearance_cm,wrap_radius_cm,initially_covered\n";
    for (const auto &p : points)
      targetCsv << p.pos[0] << ',' << p.pos[1] << ',' << p.pos[2] << ','
                << p.normal[0] << ',' << p.normal[1] << ',' << p.normal[2] << ','
                << p.clearance << ',' << p.radius << ',' << unsigned(p.covered) << '\n';
  }
  MediumGraphResult initialGraph = BuildMediumPatchGraph(
      points, origin, sampleDx, scene, scene.outputFolder + "/medium_fruit_graph_before.obj");
  LOGI("medium patches: " << points.size() << " targets (" << initiallyCovered
       << " already covered), " << initialGraph.components << " surface components, "
       << initialGraph.fruitEdges << " fruit-graph edges; masked " << filteredInner
       << " inner, " << filteredExclusion << " exclusion, " << filteredDeep
       << " deep targets; setup " << clock.ElapsedMS() / 1000.0 << " s\n");
  const double setupSeconds = clock.ElapsedMS() / 1000.0;
  LOGI("medium patches: budget " << step.count << " placements, " << maxAttempts
       << " target attempts, " << maxSeconds << " s wall time\n");

  unsigned attempts = 0, placed = 0, covered = initiallyCovered;
  MediumGraphResult activeGraph = std::move(initialGraph);
  for (unsigned round = 0; round < 12; ++round) {
    const unsigned placedBeforeRound = placed;
    const unsigned attemptsBeforeRound = attempts;
    LOGI("medium graph refinement round " << round + 1 << ": "
         << activeGraph.openComponents << " open components, largest "
         << activeGraph.largestOpenComponent << " samples\n");
  for (unsigned targetIndex : activeGraph.order) {
    const auto &target = points[targetIndex];
    if (clock.ElapsedMS() / 1000.0 >= maxSeconds || attempts >= maxAttempts ||
        placed >= step.count || placed - placedBeforeRound >= 600 ||
        attempts - attemptsBeforeRound >= 5000) break;
    if (target.covered) continue;
    attempts++;
    const double attemptStartMs = clock.ElapsedMS();
    double settleMs = 0.0;
    unsigned settleCalls = 0;
    std::vector<unsigned> selected = kinds;
    const float desiredDiag = 2.5f * target.clearance;
    std::sort(selected.begin(), selected.end(), [&](unsigned a, unsigned b) {
      return std::fabs(scene.items[a].BoxDiagonal() - desiredDiag) <
             std::fabs(scene.items[b].BoxDiagonal() - desiredDiag);
    });
    if (selected.size() > 4) {
      unsigned largest = *std::max_element(selected.begin(), selected.end(),
          [&](unsigned a, unsigned b) {
            return scene.items[a].BoxDiagonal() < scene.items[b].BoxDiagonal();
          });
      selected.resize(3);
      if (std::find(selected.begin(), selected.end(), largest) == selected.end())
        selected.push_back(largest);
    }
    struct Proposal { unsigned kind = ~0u; unsigned gain = 0; float supportGap = 0;
                      RigidTransform pose;
                      std::vector<RigidTransform> trajectory; } best;
    for (size_t ki = 0; ki < selected.size(); ++ki) {
      if (clock.ElapsedMS() / 1000.0 >= maxSeconds) break;
      const unsigned kind = selected[ki];
      MeshInfo &item = scene.items[kind];
      float supportDist;
      Vec3f away;
      if (!MediumNearestSupport(scene, target.pos, supportDist, away)) continue;
      for (unsigned rotation = 0; rotation < 2; ++rotation) {
        if (clock.ElapsedMS() / 1000.0 >= maxSeconds) break;
        Vec3f outward = target.normal;
        const Vec3f angles = scene.randAngles[(attempts * 11 + ki * 3 + rotation) % scene.randAngles.size()];
        RigidTransform start;
        start.rotation = RotationMatrixRad(angles[0], angles[1], angles[2]);
        const float supportShift = std::clamp(0.30f * item.BoxDiagonal() - supportDist - 0.2f,
                                              -3.0f, 3.0f);
        const Vec3f goal = target.pos + away * supportShift +
                           outward * (rotation == 0 ? 0.1f * item.BoxDiagonal() : 0.0f);
        start.position = goal + outward * 1.5f;
        if (ContainerOutsideFraction(item.samples, start.rotation, start.position, scene.sdf) > 0.2f ||
            !MediumOutsideExclusion(scene, item, start) ||
            SignedOverlapFraction(scene, item.box, item.samples, start.rotation, start.position) > 0.6f)
          continue;
        std::vector<RigidTransform> trajectory;
        NudgeOutcome outcome;
        Utils::Stopwatch settleClock;
        settleClock.Start();
        RigidTransform pose = scene.NudgeToTarget(kind, start, goal, trajectory, &outcome);
        const double thisSettleMs = settleClock.ElapsedMS();
        settleMs += thisSettleMs;
        settleCalls++;
        if (timer) timer->Record(item.name, thisSettleMs);
        if (ContainerOutsideFraction(item.samples, pose.rotation, pose.position, scene.sdf) >
                kContainerOutsideThresh ||
            !MediumOutsideExclusion(scene, item, pose) ||
            SignedOverlapFraction(scene, item.box, item.samples, pose.rotation, pose.position) >
                kOccupiedThresh ||
            ExistingInsideCandidateFraction(scene, item.box, pose.rotation, pose.position,
                                            item.sdf) > kOccupiedThresh) continue;
        const unsigned gain = MediumCoverageGain(points, item, pose);
        if (gain < 2 || gain < best.gain) continue;
        const float supportGap = MediumSupportGap(scene, item, pose);
        if ((gain == best.gain && best.kind != ~0u &&
             item.BoxDiagonal() <= scene.items[best.kind].BoxDiagonal()) ||
            supportGap > 0.30f ||
            MediumOverlapsTooDeeply(scene, item, pose))
          continue;
        best.kind = kind;
        best.pose = pose;
        best.gain = gain;
        best.supportGap = supportGap;
        best.trajectory = std::move(trajectory);
      }
    }
    if (best.kind == ~0u) {
      if (attempts % 50 == 0) {
        LOGI("medium patches: " << attempts << " targets tried, " << placed
             << " placed, " << covered << "/" << points.size()
             << " covered, elapsed " << clock.ElapsedMS() / 1000.0
             << " s, latest attempt " << (clock.ElapsedMS() - attemptStartMs) / 1000.0
             << " s (settle " << settleMs / 1000.0 << " s in " << settleCalls
             << " calls)\n");
      }
      continue;
    }
    unsigned id = scene.Put(best.kind, best.pose);
    scene.instances[id].trajectory = std::move(best.trajectory);
    placed++;
    for (auto &p : points) {
      if (!p.covered && MediumPointCovered(p, scene.items[best.kind], best.pose)) {
        p.covered = true;
        p.owner = id;
        covered++;
      }
    }
    const double elapsedSeconds = clock.ElapsedMS() / 1000.0;
    const double activeSeconds = elapsedSeconds - setupSeconds;
    LOGI("medium placement " << placed << "/" << step.count << " "
         << scene.items[best.kind].name << " diag "
         << scene.items[best.kind].BoxDiagonal() << " cm: gain " << best.gain
         << ", support gap " << best.supportGap << " cm, covered "
         << covered << "/" << points.size() << ", target " << attempts
         << ", attempt " << (clock.ElapsedMS() - attemptStartMs) / 1000.0
         << " s, settle " << settleMs / 1000.0 << " s/" << settleCalls
         << " calls, elapsed " << elapsedSeconds << " s, active average "
         << activeSeconds / placed << " s/placement\n");
    if (placed % 25 == 0) scene.SaveInstances(scene.packFile + "_medium_checkpoint.txt");
  }
    if (clock.ElapsedMS() / 1000.0 >= maxSeconds || attempts >= maxAttempts ||
        placed >= step.count || placed == placedBeforeRound) break;
    activeGraph = BuildMediumPatchGraph(points, origin, sampleDx, scene,
        scene.outputFolder + "/medium_fruit_graph_round" + std::to_string(round + 1) + ".obj");
  }
  scene.SaveInstances(scene.packFile + "_after_medium.txt");
  MediumGraphResult finalGraph = BuildMediumPatchGraph(
      points, origin, sampleDx, scene, scene.outputFolder + "/medium_fruit_graph_after.obj");
  std::ofstream report(scene.outputFolder + "/medium_patch_coverage.txt");
  report << "source " << cfg.ResumePackPath() << '\n'
         << "targets " << points.size() << '\n'
         << "initially_covered " << initiallyCovered << '\n'
         << "final_covered " << covered << '\n'
         << "filtered_inner " << filteredInner << '\n'
         << "filtered_exclusion " << filteredExclusion << '\n'
         << "filtered_deep " << filteredDeep << '\n'
         << "graph_components " << finalGraph.components << '\n'
         << "graph_fruit_edges " << finalGraph.fruitEdges << '\n'
         << "open_graph_sample_edges " << finalGraph.openSampleEdges << '\n'
         << "open_graph_components " << finalGraph.openComponents << '\n'
         << "largest_open_component_samples " << finalGraph.largestOpenComponent << '\n'
         << "placed " << placed << '\n'
         << "targets_tried " << attempts << '\n'
         << "elapsed_seconds " << clock.ElapsedMS() / 1000.0 << '\n';
  LOGI("medium patches: finished with " << placed << " fruit, " << covered << "/"
       << points.size() << " target samples covered in " << clock.ElapsedMS() / 1000.0
       << " s\n");
}

void PackFillCrevices(PackingScene &scene, const PackingConfig &cfg,
                      const CreviceFieldParams &fp,
                      const std::vector<std::string> &smallItemNames,
                      const CreviceBaseline &baseline, PlacementTimer *timer) {
  std::vector<unsigned> kindsBySize;
  for (const std::string &name : smallItemNames) {
    auto it = scene.nameToIndex.find(name);
    if (it != scene.nameToIndex.end()) {
      kindsBySize.push_back(it->second);
    }
  }
  if (kindsBySize.empty()) {
    std::vector<int> bySize = SortBySize(scene.items);
    if (!bySize.empty()) {
      kindsBySize.push_back(unsigned(bySize.back()));
    }
  }
  std::sort(kindsBySize.begin(), kindsBySize.end(), [&](unsigned a, unsigned b) {
    return scene.items[a].BoxDiagonal() > scene.items[b].BoxDiagonal();
  });
  if (kindsBySize.empty()) {
    LOGI("fill crevices: no small item kinds resolved, nothing to do\n");
    return;
  }

  // targets come only from the fixed baseline; the field is recomputed
  // each round only to check whether a baseline spot is already closed.
  std::vector<unsigned> attempts(baseline.spots.size(), 0);
  for (unsigned round = 0; round < cfg.creviceMaxRounds; round++) {
    size_t before = scene.instances.size();
    Utils::Stopwatch clock;
    clock.Start();

    CurrentVoid cur = ComputeCurrentVoid(scene, fp);
    if (cur.vf.dist.GetSize()[0] == 0) {
      LOGI("fill crevices round " << round << ": void field is empty, stopping\n");
      break;
    }
    EnvelopeField envelope = ComputeEnvelopeField(scene, fp.shrinkwrapVoxelSize);

    ClaimGrid claims;
    unsigned filled = 0, claimed = 0, noFit = 0, closed = 0, exhausted = 0;
    for (size_t i = 0; i < baseline.spots.size(); i++) {
      const VoidSpot &s = baseline.spots[i];
      if (OpenFraction(baseline.spotVoxels[i], cur) < cfg.creviceClosedFrac) {
        closed++;
        continue;
      }
      if (attempts[i] >= cfg.creviceMaxAttemptsPerSpot) {
        exhausted++;
        continue;
      }
      if (claims.IsClaimed(s.pos)) {
        claimed++;
        continue;
      }
      attempts[i]++;
      int id = TryFillSpot(scene, s.pos, 2.0f * s.radius, kindsBySize, cfg.creviceClearance,
                           envelope, claims, timer, /*sizeMustFitLocalWidth=*/false);
      if (id >= 0) {
        filled++;
      } else {
        noFit++;
      }
    }
    LOGI("fill crevices round " << round << ": " << baseline.spots.size() << " baseline spots, "
                                << filled << " placed, " << closed << " closed, " << claimed
                                << " claimed, " << exhausted << " out of attempts, " << noFit
                                << " no fit/settled, " << scene.instances.size()
                                << " instances total, " << (clock.ElapsedMS() / 1000.0)
                                << " s\n");
    if (scene.instances.size() - before < cfg.creviceMinPlaced) {
      LOGI("fill crevices: round " << round << " placed fewer than "
                                   << cfg.creviceMinPlaced << ", stopping\n");
      break;
    }
  }
}

CreviceCoverageReport ComputeCreviceCoverage(PackingScene &scene, const PackingConfig &cfg,
                                             const CreviceFieldParams &fp,
                                             const CreviceBaseline &baseline) {
  CreviceCoverageReport rep;
  rep.usefulVolume = cfg.creviceUsefulVolume;
  rep.spotsTotal = unsigned(baseline.spots.size());
  float voxVol = baseline.voxelSize * baseline.voxelSize * baseline.voxelSize;
  CurrentVoid cur = ComputeCurrentVoid(scene, fp);

  struct Cand {
    unsigned id;
    Vec3f pos;
    float halfDiag;
  };
  std::vector<Cand> phase3;
  for (size_t i = baseline.numBaseInstances; i < scene.instances.size(); i++) {
    const InstanceInfo &inst = scene.instances[i];
    phase3.push_back({unsigned(i), inst.tran.position,
                      0.5f * scene.items[inst.itemId].BoxDiagonal()});
  }
  std::unordered_map<unsigned, float> closedBy;
  const float ATTRIB_RANGE = 1.0f;

  for (size_t s = 0; s < baseline.spots.size(); s++) {
    const std::vector<Vec3f> &voxels = baseline.spotVoxels[s];
    unsigned open = 0;
    for (const Vec3f &p : voxels) {
      rep.c0Volume += voxVol;
      if (cur.IsOpen(p)) {
        open++;
        rep.stillOpenVolume += voxVol;
        continue;
      }
      int best = -1;
      float bestD = ATTRIB_RANGE;
      for (const Cand &c : phase3) {
        if ((p - c.pos).norm() - c.halfDiag > ATTRIB_RANGE) {
          continue;
        }
        const InstanceInfo &inst = scene.instances[c.id];
        const MeshInfo &item = scene.items[inst.itemId];
        float d;
        if (item.sdf) {
          Vec3f local = inst.tran.rotation.transposed() * (p - inst.tran.position);
          d = item.sdf->GetCoarseDist(local);
        } else {
          d = (p - c.pos).norm() - c.halfDiag;
        }
        if (d < bestD) {
          bestD = d;
          best = int(c.id);
        }
      }
      if (best >= 0) {
        closedBy[unsigned(best)] += voxVol;
      } else {
        rep.unattributedClosed += voxVol;
      }
    }
    if (voxels.empty() || float(open) / float(voxels.size()) < cfg.creviceClosedFrac) {
      rep.spotsClosed++;
    }
  }
  rep.closedVolume = rep.c0Volume - rep.stillOpenVolume;

  if (cur.vf.dist.GetSize()[0] > 0) {
    Vec3u size = cur.spots.cleanedMask.GetSize();
    float h = cur.vf.voxelSize;
    float curVoxVol = h * h * h;
    for (unsigned z = 0; z < size[2]; z++) {
      for (unsigned y = 0; y < size[1]; y++) {
        for (unsigned x = 0; x < size[0]; x++) {
          if (cur.spots.cleanedMask(x, y, z) == 0) {
            continue;
          }
          Vec3f p = cur.vf.origin + Vec3f((x + 0.5f) * h, (y + 0.5f) * h, (z + 0.5f) * h);
          if (!MaskAt(baseline.allMask, baseline.origin, baseline.voxelSize, p)) {
            rep.newVoidVolume += curVoxVol;
          }
        }
      }
    }
  }

  for (const Cand &c : phase3) {
    CreviceCoverageReport::Fruit f;
    f.instanceId = c.id;
    const MeshInfo &item = scene.items[scene.instances[c.id].itemId];
    f.itemName = item.name;
    f.pos = item.rb.GetInputTran(scene.instances[c.id].tran).position;
    auto it = closedBy.find(c.id);
    f.closedVolume = it == closedBy.end() ? 0.0f : it->second;
    if (f.closedVolume < cfg.creviceUsefulVolume) {
      rep.unnecessaryCount++;
    }
    rep.fruits.push_back(f);
  }
  std::sort(rep.fruits.begin(), rep.fruits.end(),
            [](const auto &a, const auto &b) { return a.closedVolume < b.closedVolume; });
  return rep;
}

void SaveCreviceCoverageReport(const CreviceCoverageReport &rep, const std::string &filename,
                               const std::string &objFilename) {
  std::ofstream out(filename);
  out << "baseline_spots: " << rep.spotsTotal << "\n";
  out << "baseline_spots_closed: " << rep.spotsClosed << "\n";
  out << "c0_volume_cm3: " << rep.c0Volume << "\n";
  out << "c0_still_open_cm3: " << rep.stillOpenVolume << "\n";
  out << "c0_closed_cm3: " << rep.closedVolume << "\n";
  out << "c0_closed_unattributed_cm3: " << rep.unattributedClosed << "\n";
  out << "new_void_outside_baseline_cm3: " << rep.newVoidVolume << "\n";
  out << "phase3_fruits: " << rep.fruits.size() << "\n";
  out << "unnecessary_fruits (closed < " << rep.usefulVolume << " cm3): " << rep.unnecessaryCount
      << "\n";
  out << "# fruit: instance_id item_name pos_x pos_y pos_z closed_c0_cm3 (least useful first)\n";
  std::vector<Vec3f> pts;
  for (const auto &f : rep.fruits) {
    out << "fruit: " << f.instanceId << " " << f.itemName << " " << f.pos[0] << " " << f.pos[1]
        << " " << f.pos[2] << " " << f.closedVolume << "\n";
    if (f.closedVolume < rep.usefulVolume) {
      pts.push_back(f.pos);
    }
  }
  if (!objFilename.empty()) {
    SaveVec3fObj(objFilename, pts);
  }
  LOGI("crevice coverage: C0 " << rep.c0Volume << " cm3, closed " << rep.closedVolume
                               << ", still open " << rep.stillOpenVolume << ", new void "
                               << rep.newVoidVolume << ", spots closed " << rep.spotsClosed << "/"
                               << rep.spotsTotal << ", unnecessary fruit " << rep.unnecessaryCount
                               << "/" << rep.fruits.size() << ", saved " << filename << "\n");
}

void PlacementTimer::Record(const std::string &name, double ms) {
  Stats &s = byName[name];
  s.count++;
  s.totalMs += ms;
  s.minMs = std::min(s.minMs, ms);
  s.maxMs = std::max(s.maxMs, ms);
}

void SavePlacementTimer(const PlacementTimer &timer, const std::string &filename) {
  std::vector<std::pair<std::string, PlacementTimer::Stats>> rows(
      timer.byName.begin(), timer.byName.end());
  std::sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) {
    return a.second.totalMs > b.second.totalMs;
  });
  std::ofstream out(filename);
  out << "# name count total_ms avg_ms min_ms max_ms\n";
  for (const auto &[name, s] : rows) {
    double avg = s.count > 0 ? s.totalMs / double(s.count) : 0.0;
    out << name << " " << s.count << " " << s.totalMs << " " << avg << " "
        << (s.count > 0 ? s.minMs : 0.0) << " " << s.maxMs << "\n";
  }
  LOGI("saved settle timing for " << rows.size() << " item kinds to " << filename << "\n");
}

// exact mesh volume via the divergence theorem.
static float MeshSignedVolume(const TrigMesh &mesh) {
  double vol = 0.0;
  size_t numTris = mesh.t.size() / 3;
  for (size_t i = 0; i < numTris; i++) {
    Vec3f v0 = mesh.Vert(mesh.t[3 * i + 0]);
    Vec3f v1 = mesh.Vert(mesh.t[3 * i + 1]);
    Vec3f v2 = mesh.Vert(mesh.t[3 * i + 2]);
    vol += double(v0.dot(v1.cross(v2)));
  }
  return float(std::fabs(vol) / 6.0);
}

PackQualityReport ComputePackQuality(PackingScene &scene, const PackingConfig &cfg,
                                     unsigned numOffendersToReport) {
  PackQualityReport report;
  if (!scene.sdf) {
    return report;
  }

  std::unordered_map<unsigned, float> kindVolume;
  for (const InstanceInfo &inst : scene.instances) {
    auto it = kindVolume.find(inst.itemId);
    if (it == kindVolume.end()) {
      it = kindVolume.emplace(inst.itemId, MeshSignedVolume(scene.items[inst.itemId].mesh)).first;
    }
    report.fruitVolume += it->second;
  }

  {
    Vec3u sdfSize = scene.sdf->dist.GetSize();
    double containerVoxels = 0.0;
    for (size_t i = 0; i < scene.sdf->dist.GetData().size(); i++) {
      if (float(scene.sdf->dist.GetData()[i]) * scene.sdf->distUnit <= 0.0f) {
        containerVoxels += 1.0;
      }
    }
    float sdfVoxelVolume = scene.sdf->voxSize * scene.sdf->voxSize * scene.sdf->voxSize;
    report.containerVolume = float(containerVoxels) * sdfVoxelVolume;
  }

  VoidField vf = ComputeVoidField(scene, cfg.shrinkwrapRadius, cfg.shrinkwrapVoxelSize);
  if (vf.dist.GetSize()[0] == 0) {
    return report;
  }
  float threshold = cfg.shrinkwrapVoxelSize * 1.1f;
  VoidSpotResult result = ExtractVoidSpots(vf, threshold, cfg.shrinkwrapOpenRadiusVoxels);

  float voxelVolume = cfg.shrinkwrapVoxelSize * cfg.shrinkwrapVoxelSize * cfg.shrinkwrapVoxelSize;
  Vec3u gridSize = vf.dist.GetSize();
  double openVoxels = 0.0;
  std::unordered_map<unsigned, float> nearbyOpenVolume;
  for (unsigned z = 0; z < gridSize[2]; z++) {
    for (unsigned y = 0; y < gridSize[1]; y++) {
      for (unsigned x = 0; x < gridSize[0]; x++) {
        if (result.cleanedMask(x, y, z) == 0) {
          continue;
        }
        openVoxels += 1.0;
        if (scene.instances.empty()) {
          continue;
        }
        Vec3f world = vf.origin + Vec3f((x + 0.5f) * cfg.shrinkwrapVoxelSize,
                                        (y + 0.5f) * cfg.shrinkwrapVoxelSize,
                                        (z + 0.5f) * cfg.shrinkwrapVoxelSize);
        unsigned nearest = 0;
        float bestD2 = 1e30f;
        for (unsigned i = 0; i < scene.instances.size(); i++) {
          float d2 = (scene.instances[i].tran.position - world).norm2();
          if (d2 < bestD2) {
            bestD2 = d2;
            nearest = i;
          }
        }
        nearbyOpenVolume[nearest] += voxelVolume;
      }
    }
  }
  report.openVolume = float(openVoxels) * voxelVolume;
  report.fillFraction = report.containerVolume > 0.0f
                            ? report.fruitVolume / report.containerVolume
                            : 0.0f;
  report.openFraction = report.containerVolume > 0.0f
                            ? report.openVolume / report.containerVolume
                            : 0.0f;

  std::vector<std::pair<unsigned, float>> offenders(nearbyOpenVolume.begin(),
                                                     nearbyOpenVolume.end());
  std::sort(offenders.begin(), offenders.end(),
           [](const auto &a, const auto &b) { return a.second > b.second; });
  if (offenders.size() > numOffendersToReport) {
    offenders.resize(numOffendersToReport);
  }
  for (const auto &[instId, vol] : offenders) {
    PackQualityReport::Offender off;
    off.instanceId = instId;
    off.itemName = scene.items[scene.instances[instId].itemId].name;
    off.pos = scene.instances[instId].tran.position;
    off.nearbyOpenVolume = vol;
    report.worstOffenders.push_back(off);
  }
  return report;
}

void SavePackQualityReport(const PackQualityReport &report, const std::string &filename) {
  std::ofstream out(filename);
  out << "container_volume_cm3: " << report.containerVolume << "\n";
  out << "fruit_volume_cm3: " << report.fruitVolume << "\n";
  out << "open_volume_cm3: " << report.openVolume << "\n";
  out << "fill_fraction: " << report.fillFraction << "\n";
  out << "open_fraction: " << report.openFraction << "\n";
  out << "# worst offenders: instance_id item_name pos_x pos_y pos_z nearby_open_volume_cm3\n";
  for (const auto &off : report.worstOffenders) {
    out << "offender: " << off.instanceId << " " << off.itemName << " "
        << off.pos[0] << " " << off.pos[1] << " " << off.pos[2] << " "
        << off.nearbyOpenVolume << "\n";
  }
  LOGI("saved pack quality report to " << filename << " (fill "
                                       << (report.fillFraction * 100.0f)
                                       << "%, open " << (report.openFraction * 100.0f)
                                       << "%, " << report.worstOffenders.size()
                                       << " offenders listed)\n");
}

void ComputeSurfaceDepths(PackingScene &scene,
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

  const float neighborRadius = 3.0f;
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

// prints the container-surface ray closest to targetPos plus its
// neighbors, for diagnosing a deep-ray flag without running a full pack.
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

void SeedSmallFruitCrevices(PackingScene &scene,
                            const std::vector<std::string> &smallItemNames,
                            const std::vector<Vec3f> &surfacePoints) {
  std::vector<unsigned> smallItems;
  for (const std::string &name : smallItemNames) {
    auto it = scene.nameToIndex.find(name);
    if (it != scene.nameToIndex.end()) {
      smallItems.push_back(it->second);
    }
  }
  if (smallItems.empty()) {
    std::vector<int> bySize = SortBySize(scene.items);
    if (!bySize.empty()) {
      smallItems.push_back(unsigned(bySize.back()));
    }
  }

  const unsigned MIN_SEEDED = 10;
  const unsigned MAX_ROUNDS = 20;
  for (unsigned round = 0; round < MAX_ROUNDS; round++) {
    std::vector<Vec3f> deepOrigins;
    std::vector<Vec3f> deepEnds;
    ComputeSurfaceDepths(scene, deepOrigins, deepEnds);
    unsigned seeded = SeedDeepCrevices(scene, deepOrigins, deepEnds, smallItems, surfacePoints);
    LOGI("crevice round " << round << ": seeded " << seeded << "\n");
    if (seeded < MIN_SEEDED) {
      break;
    }
  }
}

void PackScene(PackingScene &scene, const PackingPlan &plan, const PackingConfig &cfg) {
  PrepareBackground(scene, cfg);

  scene.packFile = scene.outputFolder + "/pack";
  scene.trajFile = scene.outputFolder + "/traj";
  scene.placed.resize(scene.items.size());

  PlacementTimer runTimer;

  if (scene.exclusionItemIndex != ~0u) {
    RigidTransform identity;
    unsigned instId = scene.Put(scene.exclusionItemIndex, identity);
    scene.instances[instId].isExclusion = true;
    LOGI("exclusion mesh placed at identity\n");
  }

  if (cfg.resume) {
    LoadPack(scene, cfg.ResumePackPath());
    // force samples/sdf population for resumed kinds -- LoadPack's Put()
    // does not, and downstream overlap checks need it.
    for (const InstanceInfo &inst : scene.instances) {
      scene.EnsureItemSamples(inst.itemId);
    }
  }
  std::vector<Vec3f> surfPts;
  bool haveSurfPts = false;
  bool savedBeforeFill = false;
  unsigned creviceStepCount = 0;

  size_t endStep = std::min(plan.steps.size(), size_t(cfg.endStep));
  for (size_t i = cfg.startStep; i < endStep; i++) {
    const PackingStep &step = plan.steps[i];
    LOGI("=== step " << i << " of " << plan.steps.size() << " ("
                     << (step.kind == StepKind::Crevice ? "crevice" :
                         step.kind == StepKind::MediumPatch ? "medium patches" : "bulk") << ") ===\n");
    Utils::Stopwatch clock;
    clock.Start();
    size_t before = scene.instances.size();

    if (step.kind == StepKind::MediumPatch) {
      PackMediumPatches(scene, step, cfg, &runTimer);
    } else if (step.kind == StepKind::Crevice) {
      if (step.useInnerContainer && !scene.innerContainerEnabled) {
        AddInnerContainer(scene);
      }
      if (!savedBeforeFill) {
        scene.SaveTrajectories(scene.trajFile + "_before_ray.txt");
        scene.SaveInstances(scene.packFile + "_before_ray.txt");
        Array3D<short> dist;
        Vec3f origin;
        float distUnit;
        ComputeShrinkWrapDistField(scene, 1.0f, 0.2f, dist, origin, distUnit);
        TrigMesh hull = ComputeShrinkWrapMesh(dist, origin, 0.2f, distUnit);
        SaveShrinkWrapMesh(hull, scene.outputFolder + "/shrinkwrap.obj");
        SavePackQualityReport(ComputePackQuality(scene, cfg),
                              scene.outputFolder + "/pack_quality_before_fill.txt");
        savedBeforeFill = true;
      }

      CreviceFieldParams fp;
      fp.shrinkwrapRadius = step.shrinkwrapRadius;
      fp.shrinkwrapVoxelSize = step.shrinkwrapVoxelSize;
      fp.shrinkwrapOpenRadiusVoxels = step.shrinkwrapOpenRadiusVoxels;

      CreviceBaseline baseline = ComputeCreviceBaseline(scene, cfg, fp, scene.instances.size());
      std::string suffix = "_" + std::to_string(creviceStepCount);
      {
        std::vector<Vec3f> spotPts;
        for (const VoidSpot &s : baseline.spots) {
          spotPts.push_back(s.pos);
        }
        SaveVec3fObj(scene.outputFolder + "/crevice_baseline_spots" + suffix + ".obj", spotPts);
        LOGI("crevice baseline: " << baseline.spots.size() << " in-range spots from "
                                  << baseline.numBaseInstances << " instances\n");
      }
      PackFillCrevices(scene, cfg, fp, step.names, baseline, &runTimer);
      SaveCreviceCoverageReport(ComputeCreviceCoverage(scene, cfg, fp, baseline),
                                scene.outputFolder + "/crevice_coverage" + suffix + ".txt",
                                scene.outputFolder + "/crevice_unnecessary" + suffix + ".obj");
      creviceStepCount++;
    } else if (step.useShrinkwrapSurfacePoints) {
      // recomputed fresh every time -- unlike free container points, the
      // shrinkwrap surface changes with every instance placed before this
      // step, so nothing here can be cached across steps.
      std::vector<Vec3f> shrinkPts =
          ComputeShrinkwrapSurfacePoints(scene, step.shrinkwrapRadius, step.shrinkwrapVoxelSize);
      LOGI("shrinkwrap surface: " << shrinkPts.size() << " points at radius "
                                  << step.shrinkwrapRadius << " cm, voxel "
                                  << step.shrinkwrapVoxelSize << " cm\n");
      SaveVec3fObj(scene.outputFolder + "/shrinkwrap_surface_step" + std::to_string(i) + ".obj",
                   shrinkPts);
      PackStep(scene, step, cfg, shrinkPts, &runTimer);
    } else {
      if (step.useFreeSurfacePoints && !haveSurfPts) {
        surfPts = ComputeFreeContainerPoints(scene);
        SaveVec3fObj(scene.outputFolder + "/free_container_surface.obj", surfPts);
        LOGI("free surface: " << surfPts.size() << " unoccupied points\n");
        haveSurfPts = true;
      }
      PackStep(scene, step, cfg, step.useFreeSurfacePoints ? surfPts : std::vector<Vec3f>{},
              &runTimer);
    }

    LOGI("=== step " << i << " took " << (clock.ElapsedMS() / 1000.0) << " s, "
                     << (scene.instances.size() - before) << " placed, "
                     << scene.instances.size() << " instances total ===\n");
  }

  SavePlacementTimer(runTimer, scene.outputFolder + "/timing.txt");
  // ComputePackQuality uses the small-fruit grid (0.2cm by default). On the
  // whole hand this exceeds billions of voxels and can dwarf or exhaust a
  // medium-only run. Its own patch-coverage report above is the right metric.
  bool mediumOnly = false;
  if (endStep > cfg.startStep) {
    mediumOnly = true;
    for (size_t i = cfg.startStep; i < endStep; ++i)
      mediumOnly &= plan.steps[i].kind == StepKind::MediumPatch;
  }
  // the region-limited old-way test is also partial: no whole-hand report.
  if (cfg.oldWayMedium) mediumOnly = true;
  if (!mediumOnly) {
    PackQualityReport finalReport = ComputePackQuality(scene, cfg);
    SavePackQualityReport(finalReport, scene.outputFolder + "/pack_quality_final.txt");
    LOGI("=== final quality: fill " << (finalReport.fillFraction * 100.0f)
                                    << "%, open " << (finalReport.openFraction * 100.0f)
                                    << "%, saved pack_quality_final.txt / timing.txt ===\n");
  }
}

std::vector<Vec3f> ComputeFreeContainerPoints(PackingScene &scene) {
  const float SAMPLE_EPS = 0.3f;
  const float INWARD = 2.0f;

  TrigMesh innerMesh;
  MarchingCubes(scene.sdf->dist, -INWARD, scene.sdf->distUnit,
                scene.sdf->voxSize, scene.sdf->origin, &innerMesh);

  std::vector<SamplePoint> points;
  SamplePoints(innerMesh, SAMPLE_EPS, points);

  Vec3f origin = scene.bg.GetOrigin();
  float invDx = 1.0f / scene.bg.dx;
  Vec3u gridSize = scene.bg.vox.GetSize();

  std::vector<Vec3f> free;
  for (const auto &sp : points) {
    int ix = int((sp.x[0] - origin[0]) * invDx);
    int iy = int((sp.x[1] - origin[1]) * invDx);
    int iz = int((sp.x[2] - origin[2]) * invDx);
    if (ix < 0 || iy < 0 || iz < 0 ||
        (unsigned)ix >= gridSize[0] ||
        (unsigned)iy >= gridSize[1] ||
        (unsigned)iz >= gridSize[2]) {
      continue;
    }
    if (scene.bg.vox((unsigned)ix, (unsigned)iy, (unsigned)iz) == 0) {
      free.push_back(sp.x);
    }
  }
  return free;
}

void SaveFreeContainerSurface(PackingScene &scene, const std::string &filename) {
  std::vector<Vec3f> free = ComputeFreeContainerPoints(scene);
  SaveVec3fObj(filename, free);
  LOGI("free container surface: " << free.size() << " points unoccupied, saved "
                                  << filename << "\n");
}

std::vector<Vec3f> ComputeShrinkwrapSurfacePoints(PackingScene &scene, float shrinkRadius,
                                                  float voxelSize) {
  Array3D<short> dist;
  Array3D<short> raw;
  Vec3f origin;
  float distUnit;
  ComputeShrinkWrapDistField(scene, shrinkRadius, voxelSize, dist, origin, distUnit, &raw);
  TrigMesh hull = ComputeShrinkWrapMesh(dist, origin, voxelSize, distUnit);

  std::vector<SamplePoint> samples;
  SamplePoints(hull, voxelSize, samples);
  std::vector<Vec3f> pts;
  pts.reserve(samples.size());
  // raw(x) is the unsigned distance to the nearest ACTUAL fruit surface
  // (pre-closing). Near 0 means some single fruit's own surface passes
  // right through here -- closing did not have to bridge anything, so
  // this point is just that fruit's own convex bump, not a gap between
  // fruit worth nestling into. Without this the whole closed hull samples
  // uniformly, including every single-fruit bump, which is why the
  // unfiltered version covered the entire pack instead of just the
  // valleys between fruit -- same distinction ComputeShrinkwrapField
  // already makes for Phase 2's crevice spots (PackShrinkWrap.h).
  const float kOnSkinThresh = voxelSize * 1.1f;
  for (const SamplePoint &sp : samples) {
    float rawDist = SampleDistField(raw, origin, voxelSize, distUnit, sp.x);
    if (rawDist <= kOnSkinThresh) {
      continue;
    }
    pts.push_back(sp.x);
  }
  return pts;
}

void PackFruits(const PackingPlan &plan, const PackingConfig &cfgIn) {
  PackingConfig cfg = cfgIn;
  cfg.ClampStartStep(plan.steps.size());
  PackingScene scene;
  if (cfg.sphereCover) {
    if (!BuildSphereCoverScene(scene, plan, cfg)) return;
    RunSphereCoverFast(scene, plan, cfg);
    return;
  }
  if (!BuildScene(scene, cfg)) {
    return;
  }
  PackScene(scene, plan, cfg);
  scene.SaveTrajectories(scene.trajFile + "_final.txt");
  scene.SaveInstances(scene.packFile + "_final.txt");
  
}
