#include "DebugTools.h"

#include "AdapSDF.h"
#include "MarchingCubes.h"
#include "MeshOps.h"
#include "PackShrinkWrap.h"
#include "PackingDriver.h"
#include "PackingScene.h"
#include "PointSample.h"
#include "meshutil.h"

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

  TrigMesh hull = ComputeShrinkWrapMesh(scene, shrinkRadius, voxelSize);
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
