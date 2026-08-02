#include "DebugTools.h"

#include "AdapSDF.h"
#include "MarchingCubes.h"
#include "MeshOps.h"
#include "PackingDriver.h"
#include "PackingScene.h"
#include "PointSample.h"

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
  RigidTransform newTran = scene.Nudge(0, tran, pushDir, trajectory);
  unsigned instanceId = scene.Put(0, newTran);
  scene.instances[instanceId].trajectory = trajectory;
  std::string trajFile = scene.outputFolder + "traj_debug.txt";
  scene.SaveTrajectories(trajFile);
  std::cout << "saved " << trajFile << "\n";
}
