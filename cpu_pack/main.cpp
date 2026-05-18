#include "AdapSDF.h"
#include "AdapUDF.h"
#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"
#include "FastSweep.h"
#include "FloodOutside.h"
#include "GridUtils.h"
#include "ImageIO.h"
#include "MarchingCubes.h"
#include "MeshConvo.h"
#include "MeshOps.h"
#include "PackingScene.h"
#include "PackingOps.h"
#include "PointTrigDist.h"
#include "RigidBody.h"
#include "SDFMesh.h"
#include "Stopwatch.h"
#include "TrigGrid.h"
#include "cpu_voxelizer.h"
#include "meshutil.h"
#include "pocketfft_3df.h"
#include "PointSample.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

Vec3f closestPointTriangle(const Vec3f & p, const Vec3f & a, const Vec3f & b, const Vec3f & c);

std::shared_ptr<AdapSDF> ComputeSDF(float distUnit, float h, TrigMesh &mesh) {
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

void MakeInnerMesh() {
  TrigMesh container;
  container.LoadStl("F:/meshes/fruit_hand/hands/hand4.5m.stl");
  float dx = 2.0f;
  float distUnit = 0.01f * dx;
  std::shared_ptr<AdapSDF> sdf = ComputeSDF(distUnit, dx, container);
  //auto outside = FloodOutside(sdf->dist, 0);
  Box3f box = ComputeBBox(container.v);
  Vec3f boxSize = box.vmax - box.vmin;
  TrigMesh surf;
  MarchingCubes(sdf->dist, -32, sdf->distUnit, sdf->voxSize, sdf->origin, &surf);
  surf.SaveObj("F:/meshes/fruit_hand/hand4.5m_inner.obj");
}

void PackScene(PackingScene & scene) {  
  scene.dx = 0.3;
  scene.bg.SetMeshPtr(&scene.container.mesh);
  scene.bg.Voxelize(scene.dx);  
  InvertContainer( scene.bg.vox, 1);

  Vec3f dxVec(scene.dx);  
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
  std::ofstream out(scene.outputFolder + "/pack.txt");
  bool debugging = true;
  Vec3f voxRes(scene.dx);
  Vec3f origin = scene.bg.GetOrigin();
  scene.placed.resize(scene.items.size());
  size_t maxCount = 10000;
  //large medium small - divide items into 3 groups, last group smallest
  const unsigned NUM_GROUPS = 3;
  const unsigned SMALL_KINDS=4;
  std::vector<unsigned> itemGroups[NUM_GROUPS];
  size_t totalItems = scene.sortedBySize.size();
  unsigned groupSize = (totalItems - SMALL_KINDS) / (NUM_GROUPS - 1);

  // Divide items into groups: 0=large, 1=medium, 2=small
  for (unsigned i = 0; i < scene.sortedBySize.size(); i++) {
    unsigned groupIndex = i / groupSize;
    groupIndex = std::min(2u, groupIndex);
    if(groupIndex == 2 && i < scene.sortedBySize.size() - SMALL_KINDS){
      // make sure the small group only has SMALL_KINDS fruits.
      groupIndex = 1;
    }
    itemGroups[groupIndex].push_back(scene.sortedBySize[i]);
  }

  unsigned angleIndex = 0;
  const unsigned MAX_TRIAL_COUNT = 10;

  for(unsigned g = 0; g<NUM_GROUPS; g++){
    float sdfFactor = 1.0f;
    if (g == 2) {
      // do not need small fruits inside.
      AddInnerContainer(scene);
    }
    if(g>=2){
      sdfFactor = -1.0f;
    }
    const auto & group = itemGroups[g];
    //redundant lol who cares.
    bool tryMore = true;
    while(tryMore){
      bool canPlace = false;
      //round robin
      for(unsigned i = 0;i < group.size(); i++){        
        unsigned itemIndex = group[i];
        if(scene.items[itemIndex].noMoreFit){
          continue;
        }
        bool itemPlaced = false;
        for(unsigned trial = 0; trial<MAX_TRIAL_COUNT; trial++){
          Vec3f pos;
          Vec3f rot = randAngles[angleIndex];
          angleIndex ++;                
          if(angleIndex >= randAngles.size()){
            angleIndex = 0;
          }
          bool success = AddUsingSDF( scene.bg, scene.items[itemIndex].mesh, pos, rot, scene.sdf, sdfFactor);
          if(success){
            canPlace = true;
            itemPlaced =true;
            Transformation tran;
            tran.position = pos;
            tran.rotation = rot;
            scene.placed[itemIndex].push_back(tran);
            std::string name = scene.items[itemIndex].name;
            std::string line = name + " " + tran.toString();
            out << line <<"\n";
            std::cout << line <<"\n";
            break;
          }
        }
        if (!itemPlaced) {
          scene.items[itemIndex].noMoreFit = true;
        }
      }
      if(!canPlace){
        tryMore = false;
        break;
      }
    }

    for(unsigned i = 0;i < group.size(); i++){        
      unsigned itemIndex = group[i];
      SavePackedMesh(scene, itemIndex);      
      for(auto & t:scene.placed[i]){
        t.scale = outputScale;
      }
    }
  }
}

void PackFruits() {
  std::string dataDir = "F:/meshes/fruit_hand/";
  std::string meshDir = dataDir + "fruits/";
  PackingScene scene;
  std::vector<MeshInfo> fruits = LoadAllMeshInfo(meshDir);
  scene.items = fruits;
  scene.container;
  fs::path containerFile(dataDir + "hands/hand4.5m_finger.stl");
  // fs::path containerFile(dataDir + "box5cm.obj");
  fs::path innerContainerFile(dataDir + "hands/finger_inner.stl");
  LoadMeshInfo(scene.container, containerFile);
  LoadMeshInfo(scene.containerInner, innerContainerFile);
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
  PackScene(scene);
}


int main(int argc, char * argv[]){
  std::cout<<argv[0]<<std::endl;
  PackFruits();  
  return 0;
}
