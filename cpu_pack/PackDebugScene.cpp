#include "PackDebugScene.h"
#include "AdapSDF.h"
#include "PackingScene.h"
#include "meshutil.h"
#include "MeshOps.h"
#include "GridUtils.h"
#include "PackingOps.h"
#include "FastSweep.h"
#include <iostream>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

static std::shared_ptr<AdapSDF> ComputeSDF(float distUnit, float h, TrigMesh &mesh) {
  std::shared_ptr<AdapSDF> sdf = std::make_shared<AdapSDF>();
  sdf->voxSize = h;
  sdf->band = 16;
  sdf->distUnit = distUnit;
  sdf->BuildTrigList(&mesh);
  sdf->Compress();
  mesh.ComputePseudoNormals();
  sdf->ComputeCoarseDist();
  CloseExterior(sdf->dist, sdf->MAX_DIST);
  Array3D8u frozen;
  //sdf->FastSweepCoarse(frozen);
  int band = 1000;
  FastSweepPar(sdf->dist, sdf->voxSize, distUnit, band, frozen);
  return sdf;
}

void PackDebug() {
  std::string dataDir = "F:/meshes/fruit_hand/";
  std::string meshDir = dataDir + "fruit0/";
  PackingScene scene;
  std::vector<MeshInfo> fruits = LoadAllMeshInfo(meshDir);
  scene.items = fruits;
  scene.container;
  fs::path containerFile(dataDir + "hands/finger4.8m.stl");
  // fs::path containerFile(dataDir + "box60cm.obj");
  fs::path innerContainerFile(dataDir + "hands/finger_inner4.8m.stl");
  LoadMeshInfo(scene.container, containerFile);  
  LoadMeshInfo(scene.containerInner, innerContainerFile);
  float broadPhaseDx = 2.0f;
  Box3f containerBox = scene.container.box;
  scene.broadPhase.Init(containerBox ,broadPhaseDx);
  scene.sortedBySize = SortBySize(scene.items);
  float sdfDx = 1.0f;
  float distUnit = 0.001f * sdfDx;
  std::cout << "computing container sdf \n";
  scene.sdf = ComputeSDF(distUnit, sdfDx, scene.container.mesh);
  std::cout << "computing container sdf done \n";
  scene.outputFolder = dataDir + "/out/";
  std::string packedFile = dataDir + "pack0.txt";
  //LoadPack(scene, packedFile);
  //for(unsigned i = 0;i<scene.items.size();i++){
    //SavePackedMesh(scene, i);
  //}
  scene.InitDataStructures();
  PackDebugScene(scene);
}

void PackDebugScene(PackingScene &scene) {
  scene.dx = 0.3;
  scene.bg.SetMeshPtr(&scene.container.mesh);
  scene.bg.Voxelize(scene.dx);

  Vec3f dxVec(scene.dx);
  Vec3f origin = scene.bg.GetOrigin();

  // SaveVolAsObjMesh(scene.outputFolder + "/box_vox_flood.obj", scene.bg.vox, dxVec, origin, 2);
  InvertContainer( scene.bg.vox, 1);
  // SaveVolAsObjMesh(scene.outputFolder + "/box_vox_invert.obj", scene.bg.vox, dxVec, origin, 1);
  const unsigned NUM_COPIES = 1000;
  const float outputScale = 1.05f;
  const unsigned ANGLE_TRIALS = 5;
  std::vector<Vec3f> randAngles(100);
  unsigned int seed = 123;
  std::mt19937 engine(seed);
  std::uniform_real_distribution<float> dist(0.0f, 6.2831853);
  std::uniform_int_distribution<int> intDistri(0);
  for (size_t i = 0; i < randAngles.size(); i++) {
    randAngles[i][0] = dist(engine);
    randAngles[i][1] = dist(engine);
    randAngles[i][2] = dist(engine);
  }
  std::ofstream out(scene.outputFolder + "/pack_debug.txt");
  bool debugging = true;
  Vec3f voxRes(scene.dx);
  scene.placed.resize(scene.items.size());

  float broadPhaseDx = 2.0f;
  Box3f containerBox = scene.container.box;
  scene.broadPhase.Init(containerBox ,broadPhaseDx);
  scene.sortedBySize = SortBySize(scene.items);

  unsigned angleIndex = 0;
  const unsigned MAX_TRIAL_COUNT = 2;
  float sdfFactor = 1.0f;
  // redundant lol who cares.
  bool tryMore = true;
  unsigned placeCount = 0;
  unsigned MAX_DEBUG_PLACE_COUNT = 2;
  while (tryMore) {
    bool canPlace = false;
    unsigned itemIndex = 1;
    std::string itemName = scene.items[itemIndex].name;
    if (scene.items[itemIndex].noMoreFit) {
      continue;
    }
    bool itemPlaced = false;
    for (unsigned trial = 0; trial < MAX_TRIAL_COUNT; trial++) {
      Vec3f pos;
      Vec3f rot = randAngles[angleIndex];
      angleIndex++;
      if (angleIndex >= randAngles.size()) {
        angleIndex = 0;
      }
      bool success = FindSpot( scene.bg, scene.items[itemIndex].mesh, pos, rot, scene.sdf, sdfFactor);
      if(success){
        canPlace = true;
        itemPlaced =true;
        Transformation tran;
        tran.position = pos;
        tran.rotation = RotationMatrixRad(rot[0], rot[1], rot[2]);            
        std::string name = scene.items[itemIndex].name;
        std::string line = name + " " + tran.toString();
        out << line <<"\n";
        std::cout << line <<"\n";
        // Vec3f pushDir(-1,0,0);// scene.ForceDirection(itemIndex, tran);
        Vec3f pushDir = scene.ForceDirection(itemIndex, tran);
        // TrigMesh inst0 = MakeTransformedMesh(scene.items[itemIndex].mesh, tran);
        // inst0.SaveObj(scene.outputFolder + "/" + itemName + "_" + std::to_string(placeCount) + "_start.obj");
        std::vector<Transformation> trajectory;
        Transformation newTran = scene.Nudge(itemIndex,tran,pushDir, trajectory);
        unsigned instanceId = scene.Put(itemIndex, newTran);
        scene.instances[instanceId].trajectory = trajectory;
        // debug
        TrigMesh inst = MakeTransformedMesh(scene.items[itemIndex].mesh, newTran);
        inst.SaveObj(scene.outputFolder + "/" + itemName + "_" + std::to_string(placeCount) + ".obj");
        placeCount ++;
        break;
      }
    }
    if (!itemPlaced) {
      scene.items[itemIndex].noMoreFit = true;
    }
    if (!canPlace) {
      tryMore = false;
      break;
    }
    if(placeCount >= MAX_DEBUG_PLACE_COUNT){
      break;
    }
  }

  scene.SaveTrajectories(scene.outputFolder + "/trajectories.txt");

  unsigned itemIndex = 0;
  SavePackedMesh(scene, itemIndex);
  for (auto &t : scene.placed[itemIndex]) {
    t.scale = outputScale;
  }
}
