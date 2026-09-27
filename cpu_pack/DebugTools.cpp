#include "DebugTools.h"

#include "AdapSDF.h"
#include "MarchingCubes.h"
#include "MeshOps.h"
#include "PackShrinkWrap.h"
#include "PackValidate.h"
#include "PackingDriver.h"
#include "PackingScene.h"
#include "PointSample.h"
#include "meshutil.h"

#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

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
  CreviceBaseline baseline = ComputeCreviceBaseline(scene, cfg, numBaseInstances);
  std::cout << "crevice baseline: " << baseline.spots.size() << " in-range spots from "
            << baseline.numBaseInstances << " instances\n";
  SaveCreviceCoverageReport(ComputeCreviceCoverage(scene, cfg, baseline),
                            scene.outputFolder + "/" + outPrefix + "_coverage.txt",
                            scene.outputFolder + "/" + outPrefix + "_unnecessary.obj");
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
