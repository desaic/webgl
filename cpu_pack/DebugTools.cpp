#include "DebugTools.h"

#include "AdapSDF.h"
#include "FastSweep.h"
#include "FloodOutside.h"
#include "GridUtils.h"
#include "MarchingCubes.h"
#include "MeshOps.h"
#include "PackShrinkWrap.h"
#include "PackValidate.h"
#include "PackingDriver.h"
#include "PackingScene.h"
#include "PointSample.h"
#include "meshutil.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

bool DebugCreviceRadiusSweep(const PackingConfig &cfg,
                             const std::vector<float> &radii, float voxelSize) {
  using Clock = std::chrono::steady_clock;
  const auto start = Clock::now();
  auto seconds = [](auto since) {
    return std::chrono::duration<double>(Clock::now() - since).count();
  };
  if (radii.empty() || !std::isfinite(voxelSize) || voxelSize <= 0) return false;
  for (size_t i = 0; i < radii.size(); ++i) {
    if (!std::isfinite(radii[i]) || radii[i] <= 0 ||
        std::max(2.0f, radii[i] / voxelSize) + 2 > AdapDF::MAX_BAND ||
        (i && radii[i] <= radii[i - 1])) {
      std::cerr << "sweep radii must ascend and fit the distance-field band\n";
      return false;
    }
  }
  PackingScene scene;
  if (!BuildScene(scene, cfg)) return false;
  PrepareBackground(scene, cfg);
  LoadPack(scene, cfg.ResumePackPath());
  if (scene.instances.empty()) return false;
  fs::create_directories(cfg.OutputFolder());
  std::ofstream report(cfg.OutputFolder() + "sweep.csv");
  if (!report) return false;
  report << "radius_cm,voxel_cm,hull_triangles,patch_triangles,patch_area_cm2,"
            "outside_smallest_hull_area_cm2,outside_container_area_cm2,"
            "max_surface_clearance_cm,spots,seconds\n";

  // Build once at the maximum band/padding; every radius uses this same grid.
  VoidField largest;
  largest.voxelSize = voxelSize;
  Array3D<short> raw;
  std::cout << "sweep: building shared raw field at radius " << radii.back() << "\n";
  ComputeShrinkWrapDistField(scene, radii.back(), voxelSize, largest.dist,
                            largest.origin, largest.distUnit, &raw);
  if (raw.GetData().empty()) return false;
  const double setupSeconds = seconds(start);
  std::cout << "sweep: setup " << setupSeconds << " s; grid "
            << raw.GetSize()[0] << "x" << raw.GetSize()[1] << "x"
            << raw.GetSize()[2] << "\n";
  Array3D<short> smallest;
  for (size_t ri = 0; ri < radii.size(); ++ri) {
    const auto roundStart = Clock::now();
    const float radius = radii[ri];
    std::ostringstream label;
    label << "r" << radius << "_dx" << voxelSize;
    const std::string prefix = cfg.OutputFolder() + label.str();
    VoidField closed;
    closed.origin = largest.origin;
    closed.voxelSize = voxelSize;
    closed.distUnit = largest.distUnit;
    closed.dist = ri + 1 == radii.size() ? std::move(largest.dist) :
        CloseShrinkwrapField(raw, voxelSize, radius, closed.distUnit);
    if (ri == 0) smallest = closed.dist;
    TrigMesh hull = ComputeShrinkWrapMesh(closed.dist, closed.origin,
                                        voxelSize, closed.distUnit);
    const size_t hullCount = hull.GetNumTrigs();
    if (!hullCount) return false;
    SaveShrinkWrapMesh(hull, prefix + "_hull.obj");

    // Surface patches, not blobs of bulk interior. A triangle's centroid must
    // stand off actual fruit by >1.1 voxels. These are candidates: multi-fruit
    // support at their boundaries still needs checking before actual filling.
    std::vector<size_t> patchIds, newIds;
    double area = 0, newArea = 0, outsideArea = 0;
    float maxClearance = 0;
    for (size_t ti = 0; ti < hullCount; ++ti) {
      const Triangle tri = hull.GetTriangleVerts(ti);
      const Vec3f p = (tri[0] + tri[1] + tri[2]) * (1.0f / 3.0f);
      const float clearance = SampleDistField(raw, closed.origin, voxelSize,
                                              closed.distUnit, p);
      if (clearance <= 1.1f * voxelSize) continue;
      const double triArea = hull.GetArea(unsigned(ti));
      if (scene.sdf->GetCoarseDist(p) > 0) {
        outsideArea += triArea;
        continue;
      }
      patchIds.push_back(ti);
      area += triArea;
      maxClearance = std::max(maxClearance, clearance);
      if (ri && SampleDistField(smallest, closed.origin, voxelSize,
                               closed.distUnit, p) > voxelSize) {
        newIds.push_back(ti);
        newArea += triArea;
      }
    }
    SubSet(hull, patchIds).SaveObj(prefix + "_patches.obj");
    SubSet(hull, newIds).SaveObj(prefix + "_new_patches.obj");
    hull.Clear();

    // Identical field/extraction parameters to ComputeCurrentVoid, without
    // ComputeCreviceBaseline's O(open voxels * spots) coverage attribution.
    VoidField vf = ComputeShrinkwrapField(std::move(closed), raw, 1, 1);
    VoidSpotResult extracted = ExtractVoidSpots(vf, 1.1f * voxelSize, 0);
    std::vector<Vec3f> points;
    for (const auto &spot : extracted.spots) {
      if (2 * spot.radius >= cfg.creviceMinWidth &&
          2 * spot.radius <= cfg.creviceMaxWidth &&
          scene.sdf->GetCoarseDist(spot.pos) <= 0) points.push_back(spot.pos);
    }
    SaveVec3fObj(prefix + "_points.obj", points);
    for (const char *suffix : {"_hull.obj", "_patches.obj", "_new_patches.obj", "_points.obj"}) {
      if (!fs::exists(prefix + suffix)) return false;
    }
    const double elapsed = seconds(roundStart);
    report << radius << ',' << voxelSize << ',' << hullCount << ',' << patchIds.size()
           << ',' << area << ',' << newArea << ',' << outsideArea << ',' << maxClearance
           << ',' << points.size() << ',' << elapsed << '\n';
    report.flush();
    if (!report) return false;
    std::cout << "sweep " << label.str() << ": patch area " << area
              << " cm2, new vs smallest " << newArea << " cm2, " << points.size()
              << " points, " << elapsed << " s\n";
  }
  std::ofstream notes(cfg.OutputFolder() + "sweep_notes.txt");
  notes << "Source: " << cfg.ResumePackPath() << "\nInstances: " << scene.instances.size()
        << "\nShared setup seconds: " << setupSeconds << "\nTotal seconds: " << seconds(start)
        << "\nPatch criterion: hull triangle centroid farther than 1.1 voxels from fruit."
        << "\nContainer filter: centroid signed distance <=0; coarse, not exact clipping."
        << "\nNew patches: candidate surface >1 voxel outside the smallest-radius hull."
        << "\nThis measures relocated/new bridge surfaces, not unique hole count or fill gain."
        << "\nSpot radius measures thin-mask thickness, not the physical hole opening."
        << "\nAll radii retained separately; no fine-scale target can erase a coarse patch."
        << "\nCandidates need boundary support validation before placement. No fruits moved.\n";
  return bool(notes);
}

void MakeInnerMesh(const PackingConfig &cfg, float insetVoxels) {
  TrigMesh container;
  fs::path containerPath(cfg.ContainerPath());
  if (LoadMesh(container, containerPath) != 0) {
    std::cout << "could not load " << containerPath.string() << "\n";
    return;
  }
  float dx = cfg.containerSDFDx;
  float distUnit = 0.01f * dx;
  std::shared_ptr<AdapSDF> sdf = ComputeSDF(distUnit, dx, container);
  TrigMesh surf;
  MarchingCubes(sdf->dist, -insetVoxels, sdf->distUnit, sdf->voxSize, sdf->origin, &surf);
  std::string outFile = containerPath.replace_extension().string() + "_inner.obj";
  surf.SaveObj(outFile);
  std::cout << "saved " << outFile << "\n";
}

void DebugPointSampling(MeshInfo &meshInfo, const std::string &outputFolder) {
  std::vector<SamplePoint> allFineSamples;
  float ds = 0.5f;
  float MAX_OVERLAP = 0.2f;
  SamplePoints(meshInfo.mesh, ds, allFineSamples);
  std::vector<SamplePoint> samples = DownsamplePoints(allFineSamples, ds);
  meshInfo.ComputeSDFCached();
  SavePointsObj(outputFolder + "sample_points.obj", samples);
  MovePointsInward(samples, MAX_OVERLAP, meshInfo.sdf);
  meshInfo.mesh.SaveObj(outputFolder + "/inertia_frame.obj");
  SavePointsObj(outputFolder + "moved_points.obj", samples);
  meshInfo.samples = samples;
  TrigMesh surf;
  MarchingCubes(meshInfo.sdf->dist, -0.2, meshInfo.sdf->distUnit,
                meshInfo.sdf->voxSize, meshInfo.sdf->origin, &surf);
  surf.SaveObj(outputFolder + "/debug_sdf_inner.obj");
}

void DebugNudge(const PackingConfig &cfg) {
  PackingScene scene;
  if (!BuildScene(scene, cfg)) {
    return;
  }
  PrepareBackground(scene, cfg);

  RigidTransform tran;
  tran.position = Vec3f(0, -1.8, -1.8);
  tran.rotation = RotationMatrixRad(0, 0, 0);
  Vec3f pushDir = Vec3f(-1, -1, -1);
  std::vector<RigidTransform> trajectory;
  RigidTransform newTran = scene.Nudge(0, tran, pushDir,0.5f, trajectory);
  unsigned instanceId = scene.Put(0, newTran);
  scene.instances[instanceId].trajectory = trajectory;
  std::string trajFile = scene.outputFolder + "traj_debug.txt";
  scene.SaveTrajectories(trajFile);
  std::cout << "saved " << trajFile << "\n";
}

namespace {

/// builds the scene and loads the resume pack. false if either failed, so the
/// debug tools below do not go on to save empty geometry.
bool LoadPackedScene(PackingScene &scene, const PackingConfig &cfg) {
  if (!BuildScene(scene, cfg)) {
    std::cout << "failed to build scene\n";
    return false;
  }
  // Put() stamps into bg.vox, so the background has to exist before LoadPack.
  PrepareBackground(scene, cfg);
  LoadPack(scene, cfg.ResumePackPath());
  if (scene.instances.empty()) {
    std::cout << "no instances loaded from " << cfg.ResumePackPath() << "\n";
    return false;
  }
  return true;
}

/// the same transformed meshes VoxelizeItems voxelizes, merged into one obj,
/// so voxels and hulls can be overlaid on the geometry they came from.
void SaveMergedItemMeshes(const PackingScene &scene) {
  TrigMesh merged;
  for (const auto &inst : scene.instances) {
    merged.append(MakeTransformedMesh(scene.items[inst.itemId].mesh, inst.tran));
  }
  std::string meshFile = scene.outputFolder + "/item_meshes.obj";
  merged.SaveObj(meshFile);
  std::cout << "saved " << meshFile << " (" << merged.GetNumTrigs()
            << " triangles)\n";
}

}  // namespace

void DebugItemVoxels(const PackingConfig &cfg, float voxelSize) {
  PackingScene scene;
  if (!LoadPackedScene(scene, cfg)) {
    return;
  }
  SaveMergedItemMeshes(scene);

  Vec3f origin;
  Array3D8u vox = VoxelizeItems(scene, voxelSize, origin);

  size_t numSolid = 0;
  for (uint8_t v : vox.GetData()) {
    numSolid += (v != 0);
  }
  Vec3u gridSize = vox.GetSize();
  std::cout << "voxelized " << scene.instances.size() << " instances at "
            << voxelSize << " cm into " << gridSize[0] << "x" << gridSize[1]
            << "x" << gridSize[2] << ", " << numSolid << " solid voxels, "
            << numSolid * voxelSize * voxelSize * voxelSize << " cm3\n";
  std::cout << "grid origin " << origin[0] << " " << origin[1] << " "
            << origin[2] << "\n";

  // SaveVolAsObjMesh spans voxel (i,j,k) over [i*dx, (i+1)*dx] + origin,
  // the same cell convention VoxelizeItems reports, so origin goes in as is.
  Vec3f voxRes(voxelSize, voxelSize, voxelSize);
  std::string voxFile = scene.outputFolder + "/item_voxels.obj";
  SaveVolAsObjMesh(voxFile, vox, voxRes, origin, 1);
  std::cout << "saved " << voxFile << "\n";
}

void DebugShrinkWrap(const PackingConfig &cfg, float shrinkRadius,
                     float voxelSize) {
  PackingScene scene;
  if (!LoadPackedScene(scene, cfg)) {
    return;
  }
  SaveMergedItemMeshes(scene);

  Array3D<short> dist;
  Vec3f origin;
  float distUnit;
  ComputeShrinkWrapDistField(scene, shrinkRadius, voxelSize, dist, origin, distUnit);
  TrigMesh hull = ComputeShrinkWrapMesh(dist, origin, voxelSize, distUnit);
  if (hull.GetNumTrigs() == 0) {
    std::cout << "shrinkwrap produced an empty mesh at radius " << shrinkRadius
              << " cm, voxel size " << voxelSize << " cm\n";
    return;
  }
  std::string hullFile = scene.outputFolder + "/shrinkwrap_debug.obj";
  hull.SaveObj(hullFile);
  std::cout << "saved " << hullFile << " (" << hull.GetNumTrigs()
            << " triangles, radius " << shrinkRadius << " cm, voxel size "
            << voxelSize << " cm)\n";
}

void DebugShrinkwrapAttractionPoints(const PackingConfig &cfg, float shrinkRadius,
                                     float voxelSize) {
  PackingScene scene;
  if (!LoadPackedScene(scene, cfg)) {
    return;
  }
  std::vector<Vec3f> pts = ComputeShrinkwrapSurfacePoints(scene, shrinkRadius, voxelSize);
  std::string ptsFile = scene.outputFolder + "/shrinkwrap_attraction_points_debug.obj";
  SaveVec3fObj(ptsFile, pts);
  std::cout << "saved " << ptsFile << " (" << pts.size() << " points, radius "
            << shrinkRadius << " cm, voxel size " << voxelSize << " cm)\n";
}

void DebugVoidField(const PackingConfig &cfg, float shrinkRadius, float voxelSize) {
  PackingScene scene;
  if (!LoadPackedScene(scene, cfg)) {
    return;
  }
  SaveMergedItemMeshes(scene);

  VoidField vf = ComputeVoidField(scene, shrinkRadius, voxelSize);
  if (vf.dist.GetSize()[0] == 0) {
    std::cout << "void field is empty (no instances)\n";
    return;
  }

  // iso 0 traces every zero-crossing, including the fruit envelope's own
  // residual-rounding crust (plan.txt Phase 2b) -- not a "worth filling"
  // boundary. Threshold at voxelSize+epsilon per Phase 2b instead, so only
  // genuine gaps wide enough to matter show up. VoidField is now hull-
  // relative only (container-independent, see PackShrinkWrap.h), so there
  // is no separate wall term to isolate anymore.
  float thresh = voxelSize * 1.1f;
  TrigMesh surf;
  MarchingCubes(vf.dist, thresh, vf.distUnit, vf.voxelSize, vf.origin, &surf);
  if (surf.GetNumTrigs() == 0) {
    std::cout << "void field produced an empty surface at shrinkRadius "
              << shrinkRadius << " cm, voxelSize " << voxelSize << " cm, threshold "
              << thresh << " cm\n";
    return;
  }
  std::string outFile = scene.outputFolder + "/voidfield_debug.obj";
  surf.SaveObj(outFile);
  std::cout << "saved " << outFile << " (" << surf.GetNumTrigs()
            << " triangles, shrinkRadius " << shrinkRadius << " cm, voxelSize "
            << voxelSize << " cm)\n";
}

namespace {

// cleanedMask is 0/1 (Array3D8u); MarchingCubes wants Array3D<short>. Scale
// to 0/100 so a level of 50 with distUnit=1 traces the mask's own boundary
// -- the underlying data is a step function, not a smooth field, so this
// necessarily produces blocky/voxel-aligned blobs, which is the point: one
// isolated chunk per surviving crevice/void, not a smoothed sculpture.
TrigMesh MarchMaskBlobs(const Array3D8u &mask, float voxelSize, const Vec3f &origin) {
  Vec3u size = mask.GetSize();
  Array3D<short> field;
  field.Allocate(size, 0);
  for (size_t i = 0; i < mask.GetData().size(); i++) {
    field.GetData()[i] = mask.GetData()[i] != 0 ? short(100) : short(0);
  }
  TrigMesh surf;
  MarchingCubes(field, 50.0f, 1.0f, voxelSize, origin, &surf);
  return surf;
}

}  // namespace

void DebugVoidSpots(const PackingConfig &cfg, float shrinkRadius, float voxelSize,
                    unsigned openRadiusVoxels) {
  PackingScene scene;
  if (!LoadPackedScene(scene, cfg)) {
    return;
  }
  SaveMergedItemMeshes(scene);

  VoidField vf = ComputeVoidField(scene, shrinkRadius, voxelSize);
  if (vf.dist.GetSize()[0] == 0) {
    std::cout << "void field is empty (no instances)\n";
    return;
  }

  float threshold = voxelSize * 1.1f;
  VoidSpotResult result = ExtractVoidSpots(vf, threshold, openRadiusVoxels);
  std::cout << "void spots: " << result.spots.size() << " accepted after threshold "
            << threshold << " cm, open radius " << openRadiusVoxels << " voxel(s)\n";
  for (size_t i = 0; i < result.spots.size(); i++) {
    const VoidSpot &s = result.spots[i];
    std::cout << "  spot " << i << " pos=(" << s.pos[0] << "," << s.pos[1] << ","
              << s.pos[2] << ") radius=" << s.radius << " cm\n";
  }

  // M1: bucket by opening width (diameter = 2*radius) per plan.txt FINAL
  // GOAL's four manufacturability ranges, plus an inscribed-sphere volume
  // sum per bucket -- MULTI-SCALE FILL's own go/no-go check for a second
  // blueberry tier is "sum void volume in the 0.8-1.6cm bucket, see if it is
  // a small fraction of total", which this gives directly instead of a
  // guess.
  {
    const float BUCKET_EDGES[3] = {0.2f, 1.0f, 3.0f};
    size_t bucketCount[4] = {0, 0, 0, 0};
    float bucketVol[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float totalVol = 0.0f;
    for (const VoidSpot &s : result.spots) {
      float width = 2.0f * s.radius;
      unsigned b = 3;
      for (unsigned i = 0; i < 3; i++) {
        if (width < BUCKET_EDGES[i]) {
          b = i;
          break;
        }
      }
      float vol = (4.0f / 3.0f) * 3.14159265f * s.radius * s.radius * s.radius;
      bucketCount[b]++;
      bucketVol[b] += vol;
      totalVol += vol;
    }
    const char *labels[4] = {"<0.2cm (ignore, self-fills)",
                             "0.2-1cm (minimize count, no undercut)",
                             "1-3cm (must survive, no overhang)",
                             ">3cm (real void, Phase 2's problem)"};
    std::cout << "M1 histogram (opening width = 2*radius, total inscribed "
                 "volume "
              << totalVol << " cm3):\n";
    for (unsigned b = 0; b < 4; b++) {
      float pct = totalVol > 0.0f ? 100.0f * bucketVol[b] / totalVol : 0.0f;
      std::cout << "  " << labels[b] << ": " << bucketCount[b] << " spots, "
                << bucketVol[b] << " cm3 (" << pct << "%)\n";
    }
  }

  TrigMesh blobs = MarchMaskBlobs(result.cleanedMask, vf.voxelSize, vf.origin);
  if (blobs.GetNumTrigs() > 0) {
    std::string blobFile = scene.outputFolder + "/voidspots_blobs_debug.obj";
    blobs.SaveObj(blobFile);
    std::cout << "saved " << blobFile << " (" << blobs.GetNumTrigs() << " triangles)\n";
  } else {
    std::cout << "cleaned mask is empty after threshold+open -- nothing to draw\n";
  }

  std::vector<Vec3f> points;
  points.reserve(result.spots.size());
  for (const VoidSpot &s : result.spots) {
    points.push_back(s.pos);
  }
  std::string pointFile = scene.outputFolder + "/voidspots_points_debug.obj";
  SaveVec3fObj(pointFile, points);
  std::cout << "saved " << pointFile << " (" << points.size() << " points)\n";
}

void DebugFillVoids(const PackingConfig &cfg, float shrinkRadius, float voxelSize,
                    unsigned openRadiusVoxels, float clearance) {
  PackingScene scene;
  if (!LoadPackedScene(scene, cfg)) {
    return;
  }
  unsigned beforeCount = unsigned(scene.instances.size());

  // kindsBySize: every item in the scene, largest BoxDiagonal first. This
  // IS "nominal size only" per step 8 -- there is no second scale tier item
  // yet (MULTI-SCALE FILL's own M1-histogram check said one is not worth
  // building yet, see the void spots histogram from this same round).
  std::vector<unsigned> kindsBySize(scene.items.size());
  for (unsigned i = 0; i < scene.items.size(); i++) {
    kindsBySize[i] = i;
  }
  std::sort(kindsBySize.begin(), kindsBySize.end(), [&](unsigned a, unsigned b) {
    return scene.items[a].BoxDiagonal() > scene.items[b].BoxDiagonal();
  });

  VoidField vf = ComputeVoidField(scene, shrinkRadius, voxelSize);
  if (vf.dist.GetSize()[0] == 0) {
    std::cout << "void field is empty (no instances)\n";
    return;
  }
  float threshold = voxelSize * 1.1f;
  VoidSpotResult result = ExtractVoidSpots(vf, threshold, openRadiusVoxels);
  std::cout << "fill pass: " << result.spots.size()
            << " candidate spots, widest first\n";

  // same placed instances, same fine grid VoidField came from -- used by
  // TryFillSpot to validate a candidate pose instead of the much coarser
  // scene.bg.vox (see EnvelopeField comment in PackShrinkWrap.h).
  EnvelopeField envelope = ComputeEnvelopeField(scene, voxelSize);

  ClaimGrid claims;
  unsigned filled = 0, claimed = 0, noFit = 0;
  for (const VoidSpot &s : result.spots) {
    if (claims.IsClaimed(s.pos)) {
      claimed++;
      continue;
    }
    float localWidth = 2.0f * s.radius;
    int id = TryFillSpot(scene, s.pos, localWidth, kindsBySize, clearance, envelope, claims);
    if (id >= 0) {
      filled++;
    } else {
      noFit++;
    }
  }
  std::cout << "fill pass: " << filled << " placed, " << claimed
            << " already claimed, " << noFit << " no kind fit/settled, "
            << "instances " << beforeCount << " -> " << scene.instances.size() << "\n";

  SaveMergedItemMeshes(scene);
  std::string packFile = scene.outputFolder + "/pack_after_fill.txt";
  scene.SaveInstances(packFile);
  std::cout << "saved " << packFile << "\n";
}

void DebugCreviceCoverage(const PackingConfig &cfg, size_t numBaseInstances,
                          const std::string &outPrefix) {
  PackingScene scene;
  if (!LoadPackedScene(scene, cfg)) {
    return;
  }
  for (const InstanceInfo &inst : scene.instances) {
    scene.EnsureItemSamples(inst.itemId);
  }
  CreviceFieldParams fp;
  fp.shrinkwrapRadius = cfg.shrinkwrapRadius;
  fp.shrinkwrapVoxelSize = cfg.shrinkwrapVoxelSize;
  fp.shrinkwrapOpenRadiusVoxels = cfg.shrinkwrapOpenRadiusVoxels;
  CreviceBaseline baseline = ComputeCreviceBaseline(scene, cfg, fp, numBaseInstances);
  std::cout << "crevice baseline: " << baseline.spots.size() << " in-range spots from "
            << baseline.numBaseInstances << " instances\n";
  SaveCreviceCoverageReport(ComputeCreviceCoverage(scene, cfg, fp, baseline),
                            scene.outputFolder + "/" + outPrefix + "_coverage.txt",
                            scene.outputFolder + "/" + outPrefix + "_unnecessary.obj");
}

void DebugCreviceSpots(const PackingConfig &cfg, const CreviceFieldParams &fp,
                       bool saveItemMeshes) {
  PackingScene scene;
  if (!LoadPackedScene(scene, cfg)) {
    return;
  }
  for (const InstanceInfo &inst : scene.instances) {
    scene.EnsureItemSamples(inst.itemId);
  }
  if (saveItemMeshes) {
    SaveMergedItemMeshes(scene);
  }

  {
    Array3D<short> dist;
    Vec3f origin;
    float distUnit;
    ComputeShrinkWrapDistField(scene, fp.shrinkwrapRadius, fp.shrinkwrapVoxelSize, dist, origin,
                               distUnit);
    TrigMesh hull = ComputeShrinkWrapMesh(dist, origin, fp.shrinkwrapVoxelSize, distUnit);
    if (hull.GetNumTrigs() > 0) {
      std::string hullFile = scene.outputFolder + "/shrinkwrap_debug.obj";
      SaveShrinkWrapMesh(hull, hullFile);
    } else {
      std::cout << "shrinkwrap produced an empty mesh at radius " << fp.shrinkwrapRadius
                << " cm, voxel size " << fp.shrinkwrapVoxelSize << " cm\n";
    }
  }

  CreviceBaseline baseline = ComputeCreviceBaseline(scene, cfg, fp, scene.instances.size());
  std::cout << "crevice spots: " << baseline.spots.size() << " in-range ("
            << cfg.creviceMinWidth << "-" << cfg.creviceMaxWidth << " cm wide) from "
            << baseline.numBaseInstances << " instances, shrinkwrap radius "
            << fp.shrinkwrapRadius << " cm, voxel size " << fp.shrinkwrapVoxelSize
            << " cm, erode " << fp.shrinkwrapOpenRadiusVoxels << " voxel(s)\n";
  for (size_t i = 0; i < baseline.spots.size(); i++) {
    const VoidSpot &s = baseline.spots[i];
    std::cout << "  spot " << i << " pos=(" << s.pos[0] << "," << s.pos[1] << ","
              << s.pos[2] << ") radius=" << s.radius << " cm\n";
  }

  // same M1 bucketing DebugVoidSpots prints, so the two are directly
  // comparable even though this one runs the real Phase 2 field.
  {
    const float BUCKET_EDGES[3] = {0.2f, 1.0f, 3.0f};
    size_t bucketCount[4] = {0, 0, 0, 0};
    float bucketVol[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float totalVol = 0.0f;
    for (const VoidSpot &s : baseline.spots) {
      float width = 2.0f * s.radius;
      unsigned b = 3;
      for (unsigned i = 0; i < 3; i++) {
        if (width < BUCKET_EDGES[i]) {
          b = i;
          break;
        }
      }
      float vol = (4.0f / 3.0f) * 3.14159265f * s.radius * s.radius * s.radius;
      bucketCount[b]++;
      bucketVol[b] += vol;
      totalVol += vol;
    }
    const char *labels[4] = {"<0.2cm (ignore, self-fills)",
                             "0.2-1cm (minimize count, no undercut)",
                             "1-3cm (must survive, no overhang)",
                             ">3cm (real void, Phase 2's problem)"};
    std::cout << "M1 histogram (opening width = 2*radius, total inscribed volume "
              << totalVol << " cm3):\n";
    for (unsigned b = 0; b < 4; b++) {
      float pct = totalVol > 0.0f ? 100.0f * bucketVol[b] / totalVol : 0.0f;
      std::cout << "  " << labels[b] << ": " << bucketCount[b] << " spots, "
                << bucketVol[b] << " cm3 (" << pct << "%)\n";
    }
  }

  std::vector<Vec3f> points;
  points.reserve(baseline.spots.size());
  for (const VoidSpot &s : baseline.spots) {
    points.push_back(s.pos);
  }
  std::string pointFile = scene.outputFolder + "/crevice_baseline_spots_debug.obj";
  SaveVec3fObj(pointFile, points);
  std::cout << "saved " << pointFile << " (" << points.size() << " points)\n";
}

void DebugValidatePlacementRange(const PackingConfig &cfg,
                                 const std::string &packFile,
                                 size_t rangeStart, size_t rangeEnd) {
  PackingScene scene;
  if (!BuildScene(scene, cfg)) {
    return;
  }
  PrepareBackground(scene, cfg);
  LoadPack(scene, packFile);
  if (scene.instances.empty()) {
    std::cout << "no instances loaded from " << packFile << "\n";
    return;
  }
  rangeEnd = std::min(rangeEnd, scene.instances.size());
  if (rangeStart >= rangeEnd) {
    std::cout << "empty range [" << rangeStart << "," << rangeEnd
              << ") of " << scene.instances.size() << " instances\n";
    return;
  }

  unsigned violations = 0;
  float worstOverlap = 0.0f;
  float worstOutside = 0.0f;
  for (size_t i = rangeStart; i < rangeEnd; i++) {
    // "real placement from pack file" pattern from BenchValidateSelfTest:
    // rebuild the fruit grid WITHOUT instance i, so its own voxels cannot
    // read as an overlap with itself.
    PackValidator v;
    v.Init(scene, cfg);
    for (size_t j = 0; j < scene.instances.size(); j++) {
      if (j == i) {
        continue;
      }
      v.AddPlaced(scene, scene.instances[j].itemId, scene.instances[j].tran);
    }
    ValidationResult res = v.ValidatePlacement(scene, scene.instances[i].itemId,
                                               scene.instances[i].tran);
    if (!res.Ok()) {
      violations++;
    }
    worstOverlap = std::max(worstOverlap, res.maxOverlapDepth);
    worstOutside = std::max(worstOutside, res.maxOutsideDepth);
    std::cout << "  instance " << i << " ("
              << scene.items[scene.instances[i].itemId].name << ") "
              << res.toString() << "\n";
  }
  std::cout << "validated [" << rangeStart << "," << rangeEnd << ") of "
            << scene.instances.size() << " instances from " << packFile
            << ": " << violations << " with a violation, worst overlap depth "
            << worstOverlap << " cm, worst outside depth " << worstOutside
            << " cm\n";
}

// builds a signed distance field for a mesh. negative = inside.
static std::shared_ptr<AdapSDF> BuildExclusionSDF(TrigMesh &mesh, float h) {
  const float distUnit = 0.01f;
  auto sdf = std::make_shared<AdapSDF>();
  sdf->voxSize = h;
  sdf->band = 16;
  sdf->distUnit = distUnit;
  sdf->BuildTrigList(&mesh);
  sdf->Compress();
  mesh.ComputePseudoNormals();
  sdf->ComputeCoarseDist();
  CloseExterior(sdf->dist, sdf->MAX_DIST);
  Array3D8u frozen;
  FastSweepPar(sdf->dist, h, distUnit, 1000, frozen);
  return sdf;
}

// fraction of the item's occupied voxels (at voxelSize resolution) whose
// centers land inside the exclusion SDF (dist <= 0).
static float ExclusionOverlapFraction(const TrigMesh &transformedMesh,
                                      const AdapSDF &exclSdf, float voxelSize) {
  Box3f bbox = ComputeBBox(transformedMesh.v);
  VoxConf conf;
  conf.origin = ToArray(AlignOriginToGrid(bbox.vmin, voxelSize));
  conf.unit = {voxelSize, voxelSize, voxelSize};
  conf.gridSize = ComputeGridSize(bbox, voxelSize, 1);
  Array3D8u vox;
  vox.Allocate(conf.gridSize, 0);
  VoxelizeMesh(transformedMesh, vox, conf);
  FloodOutside8u(vox, 1, 2);

  Vec3f origin(conf.origin[0], conf.origin[1], conf.origin[2]);
  Vec3u size = conf.gridSize;
  unsigned total = 0, inside = 0;
  for (unsigned z = 0; z < size[2]; z++)
    for (unsigned y = 0; y < size[1]; y++)
      for (unsigned x = 0; x < size[0]; x++) {
        if (vox(x, y, z) == 0) continue;
        total++;
        Vec3f p = origin + Vec3f((x + 0.5f) * voxelSize,
                                 (y + 0.5f) * voxelSize,
                                 (z + 0.5f) * voxelSize);
        if (exclSdf.GetCoarseDist(p) <= 0.0f) inside++;
      }
  return total == 0 ? 0.0f : float(inside) / float(total);
}

// like FilterPackByExclusion but only checks instances starting at fromLine
// (0-indexed in the pack file), keeps everything before that untouched.
// obstacles: list of (meshPath, threshold) pairs. an instance is removed if
// its overlap fraction exceeds the threshold for ANY obstacle.
void FilterPackFromIndex(const PackingConfig &cfg,
                         const std::string &packFile,
                         const std::string &trajFile,
                         const std::vector<std::pair<std::string,float>> &obstacles,
                         int fromLine,
                         const std::string &outPackSuffix,
                         const std::string &outTrajSuffix) {
  PackingScene scene;
  if (!BuildScene(scene, cfg)) return;

  std::vector<std::shared_ptr<AdapSDF>> sdfs;
  std::vector<float> thresholds;
  for (const auto &obs : obstacles) {
    TrigMesh mesh;
    if (LoadMesh(mesh, fs::path(obs.first)) != 0) {
      std::cout << "cannot load obstacle mesh: " << obs.first << "\n";
      return;
    }
    std::cout << "building obstacle SDF from " << obs.first << "\n";
    sdfs.push_back(BuildExclusionSDF(mesh, cfg.containerSDFDx));
    thresholds.push_back(obs.second);
  }

  std::ifstream packIn(packFile);
  if (!packIn.good()) { std::cout << "cannot open: " << packFile << "\n"; return; }
  int packCount = 0;
  packIn >> packCount;
  packIn.ignore(1024, '\n');

  struct Entry { std::string line; bool keep; };
  std::vector<Entry> entries;
  entries.reserve(packCount);

  const float kVox = 1.0f;
  int removed = 0;
  for (int k = 0; k < packCount; k++) {
    std::string line;
    while (line.empty()) std::getline(packIn, line);

    bool keep = true;
    if (k >= fromLine) {
      std::istringstream ls(line);
      std::string name; ls >> name;
      RigidTransform tran;
      std::string tok; float vals[9];
      ls >> tok;
      for (int i = 0; i < 3; i++) { float v; ls >> v; tran.position[i] = v; }
      ls >> tok;
      for (int i = 0; i < 9; i++) ls >> vals[i];
      tran.rotation = Matrix3f(vals[0],vals[1],vals[2],vals[3],vals[4],vals[5],vals[6],vals[7],vals[8]);
      ls >> tok; ls >> tran.scale;

      unsigned itemIdx = scene.GetItemIndex(name);
      if (itemIdx < scene.items.size()) {
        TrigMesh xf = MakeTransformedMesh(scene.items[itemIdx].mesh, tran);
        for (size_t si = 0; si < sdfs.size() && keep; si++) {
          float frac = ExclusionOverlapFraction(xf, *sdfs[si], kVox);
          if (frac > thresholds[si]) {
            keep = false; removed++;
            std::cout << "  remove [" << k << "] " << name << " obstacle=" << si
                      << " overlap=" << frac << "\n";
          }
        }
      }
    }
    entries.push_back({line, keep});
  }
  std::cout << "removed " << removed << " / " << packCount << " (checked " << (packCount - fromLine) << ")\n";

  // write filtered pack (no exclusion instance header, just fruits)
  std::string outPack = cfg.OutputFolder() + outPackSuffix;
  {
    int kept = packCount - removed;
    std::ofstream out(outPack);
    out << kept << "\n";
    for (const auto &e : entries) if (e.keep) out << e.line << "\n";
  }
  std::cout << "saved " << outPack << "\n";

  // filter trajectory: instance 0 = exclusion, instances 1..N = pack lines 0..N-1
  std::ifstream trajIn(trajFile);
  if (!trajIn.good()) { std::cout << "cannot open: " << trajFile << "\n"; return; }
  std::string outTraj = cfg.OutputFolder() + outTrajSuffix;
  std::ofstream trajOut(outTraj);

  std::string curLine;
  int newIdx = 0, instIdx = -1;
  std::vector<std::string> block;
  auto flushBlock = [&]() {
    if (block.empty()) return;
    // instIdx 0 = exclusion (always keep); instIdx k corresponds to pack line k-1
    int packLine = instIdx - 1;
    bool keep = (instIdx == 0) || (packLine >= 0 && packLine < (int)entries.size() && entries[packLine].keep);
    if (keep) {
      std::string rest = block[0].substr(block[0].find(' ', block[0].find(' ')+1));
      trajOut << "Instance " << newIdx++ << rest << "\n";
      for (size_t i = 1; i < block.size(); i++) trajOut << block[i] << "\n";
      trajOut << "\n";
    }
    block.clear();
  };
  while (std::getline(trajIn, curLine)) {
    if (curLine.size() >= 8 && curLine.substr(0,8) == "Instance") {
      flushBlock();
      std::istringstream ls(curLine); std::string tok; ls >> tok >> instIdx;
    }
    if (!curLine.empty()) block.push_back(curLine);
  }
  flushBlock();
  std::cout << "saved " << outTraj << "\n";
}

void FilterPackByExclusion(const PackingConfig &cfg,
                           const std::string &packFile,
                           const std::string &trajFile,
                           float overlapThresh) {
  // load items so we can look up mesh paths by name
  PackingScene scene;
  if (!BuildScene(scene, cfg)) return;

  // build exclusion mesh SDF
  std::string exclPath = cfg.ExclusionMeshPath();
  TrigMesh exclMesh;
  if (LoadMesh(exclMesh, fs::path(exclPath)) != 0) {
    std::cout << "exclusion mesh not found: " << exclPath << "\n";
    return;
  }
  std::cout << "building exclusion SDF from " << exclPath << "\n";
  auto exclSdf = BuildExclusionSDF(exclMesh, cfg.containerSDFDx);

  // read pack file
  std::ifstream packIn(packFile);
  if (!packIn.good()) {
    std::cout << "cannot open pack file: " << packFile << "\n";
    return;
  }
  int packCount = 0;
  packIn >> packCount;
  packIn.ignore(1024, '\n');

  struct FruitEntry { std::string line; bool keep; };
  std::vector<FruitEntry> fruits;
  fruits.reserve(packCount);

  const float kVoxelSize = 1.0f;  // 1 cm for overlap check
  int removed = 0;
  for (int k = 0; k < packCount; k++) {
    std::string line;
    std::getline(packIn, line);
    if (line.empty()) { k--; continue; }

    // parse name and transform
    std::istringstream ls(line);
    std::string name;
    ls >> name;
    RigidTransform tran;
    std::string tok;
    float vals[9];
    ls >> tok;  // "pos"
    for (int i = 0; i < 3; i++) { float v; ls >> v; tran.position[i] = v; }
    ls >> tok;  // "rot"
    for (int i = 0; i < 9; i++) ls >> vals[i];
    tran.rotation = Matrix3f(vals[0],vals[1],vals[2],vals[3],vals[4],vals[5],vals[6],vals[7],vals[8]);
    ls >> tok; ls >> tran.scale;  // "scale" val

    unsigned itemIdx = scene.GetItemIndex(name);
    bool keep = true;
    if (itemIdx < scene.items.size()) {
      TrigMesh xformed = MakeTransformedMesh(scene.items[itemIdx].mesh, tran);
      float frac = ExclusionOverlapFraction(xformed, *exclSdf, kVoxelSize);
      if (frac > overlapThresh) {
        keep = false;
        removed++;
        std::cout << "  remove " << name << " overlap=" << frac << "\n";
      }
    }
    fruits.push_back({line, keep});
  }
  std::cout << "removed " << removed << " / " << packCount << " fruits\n";

  // write filtered pack
  std::string outPack = cfg.OutputFolder() + "pack_filtered.txt";
  {
    int kept = packCount - removed;
    std::ofstream out(outPack);
    out << kept << "\n";
    for (const auto &f : fruits)
      if (f.keep) out << f.line << "\n";
  }
  std::cout << "saved filtered pack to " << outPack << "\n";

  // filter trajectory: parse instance blocks, re-number kept ones 0,1,...
  std::ifstream trajIn(trajFile);
  if (!trajIn.good()) {
    std::cout << "cannot open trajectory: " << trajFile << "\n";
    return;
  }
  std::string outTraj = cfg.OutputFolder() + "traj_filtered.txt";
  std::ofstream trajOut(outTraj);

  // instance 0 = exclusion mesh (always output as instance 0)
  // instances 1..N = fruits matching fruits[0..N-1]
  std::string curLine;
  int newIdx = 0;
  int instIdx = -1;
  std::vector<std::string> block;
  auto flushBlock = [&]() {
    if (block.empty()) return;
    // instIdx 0 = exclusion: always keep, emit as-is with new index
    bool keep = (instIdx == 0) || (instIdx >= 1 && instIdx-1 < (int)fruits.size() && fruits[instIdx-1].keep);
    if (keep) {
      // replace "Instance N" with "Instance newIdx"
      std::string header = "Instance " + std::to_string(newIdx);
      std::string rest = block[0].substr(block[0].find(' ', block[0].find(' ')+1));
      trajOut << header << rest << "\n";
      for (size_t i = 1; i < block.size(); i++) trajOut << block[i] << "\n";
      trajOut << "\n";
      newIdx++;
    }
    block.clear();
  };

  while (std::getline(trajIn, curLine)) {
    if (curLine.substr(0, 8) == "Instance") {
      flushBlock();
      std::istringstream ls(curLine);
      std::string tok; int idx;
      ls >> tok >> idx;
      instIdx = idx;
    }
    if (!curLine.empty()) block.push_back(curLine);
  }
  flushBlock();
  std::cout << "saved filtered trajectory to " << outTraj << "\n";
}
