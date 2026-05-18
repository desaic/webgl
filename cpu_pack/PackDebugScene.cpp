
#include "PackingScene.h"
#include "meshutil.h"
#include "MeshOps.h"
#include "GridUtils.h"
#include "PackingOps.h"

#include <iostream>
#include <fstream>
#include <random>

void PackDebugScene(PackingScene &scene) {
    //debug
  // std::string meshDir = dataDir + "fruit0/";
  scene.dx = 0.3;
  scene.bg.SetMeshPtr(&scene.container.mesh);
  scene.bg.Voxelize(scene.dx);

  Vec3f dxVec(scene.dx);
  Vec3f origin = scene.bg.GetOrigin();

  SaveVolAsObjMesh(scene.outputFolder + "/box_vox_flood.obj", scene.bg.vox, dxVec, origin, 2);
  InvertContainer( scene.bg.vox, 1);
  SaveVolAsObjMesh(scene.outputFolder + "/box_vox_invert.obj", scene.bg.vox, dxVec, origin, 1);
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

  unsigned angleIndex = 0;
  const unsigned MAX_TRIAL_COUNT = 10;
  float sdfFactor = -1.0f;
  // redundant lol who cares.
  bool tryMore = true;
  while (tryMore) {
    bool canPlace = false;
    unsigned itemIndex = 0;
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
      bool success = FindSpot(scene.bg, scene.items[itemIndex].mesh, pos, rot, scene.sdf, sdfFactor);
      if (success) {
        canPlace = true;
        itemPlaced = true;
        Transformation tran;
        tran.position = pos;
        tran.rotation = rot;
        scene.placed[itemIndex].push_back(tran);
        std::string name = scene.items[itemIndex].name;
        std::string line = name + " " + tran.toString();
        out << line << "\n";
        std::cout << line << "\n";
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
  }

  unsigned itemIndex = 0;
  SavePackedMesh(scene, itemIndex);
  for (auto &t : scene.placed[itemIndex]) {
    t.scale = outputScale;
  }
  SaveVolAsObjMesh("F:/meshes/fruit_hand/pack_vox.obj", scene.bg.vox, dxVec, origin, 1);
}
