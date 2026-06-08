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
#include "MeshOptimization.h"
#include "pocketfft_3df.h"
#include "PointSample.h"
#include "PackingPlan.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

Vec3f closestPointTriangle(const Vec3f & p, const Vec3f & a, const Vec3f & b, const Vec3f & c);

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

void PackStep(PackingScene & scene, const PackingStep & step){
  unsigned count = 0;
  // first item to consider in the next iteration.
  unsigned startNameIndex = 0;
  if(step.names.size() == 0){
    return;
  }

  const unsigned MAX_TRIAL_COUNT = 10;
  unsigned angleIndex = 0;
  unsigned numItems = step.names.size();
  float sdfFactor = step.outwards? 1.0f : -1.0f;
  if(step.useInnerContainer){
    AddInnerContainer(scene);
  }

  for (; count < step.count; count++) {
    bool packSuccess = false;
    for (unsigned i = 0; i < numItems; i++) {
      unsigned nameIndex = (i + startNameIndex) % numItems;
      std::string name = step.names[nameIndex];
      unsigned itemIndex = scene.GetItemIndex(name);
      MeshInfo &item = scene.items[itemIndex];
      if (item.noMoreFit) {
        continue;
      }
      bool itemPlaced = false;
      for (unsigned trial = 0; trial < MAX_TRIAL_COUNT; trial++) {
        Vec3f pos;
        Vec3f rot = scene.randAngles[angleIndex];
        angleIndex++;
        if (angleIndex >= scene.randAngles.size()) {
          angleIndex = 0;
        }
        bool success = FindSpot(scene.bg, item.mesh, pos, rot, scene.sdf, sdfFactor);
        if (success) {
          packSuccess = true;
          itemPlaced = true;
          Transformation tran;
          tran.position = pos;
          tran.rotation = RotationMatrixRad(rot[0], rot[1], rot[2]);

          Vec3f pushDir = scene.ForceDirection(itemIndex, step.force, sdfFactor, tran);
          std::vector<Transformation> trajectory;
          Transformation newTran = scene.Nudge(itemIndex,tran,pushDir, trajectory);
          unsigned instanceId = scene.Put(itemIndex, newTran);
          scene.instances[instanceId].trajectory = trajectory;
          // debug save trajectory progress
          if(count % 10 == 0 && count > 0){
            std::string trajFile = scene.trajFile + std::to_string(int(count/10) % 10) + ".txt";
            scene.SaveTrajectories(trajFile);
          }
          // debug save instances progress
          if( count % 20 == 0 && count > 0){
            std::string packFile = scene.packFile + std::to_string(int(count/20) % 10) + ".txt";
            std::ofstream out(packFile);
            scene.SaveInstances(packFile);
          }
          count ++;
          break;
        }
      }
      if(!itemPlaced){
        item.noMoreFit = true;
      }
    }
    if (!packSuccess) {
      break;
    }
    startNameIndex = (startNameIndex + 1) % numItems;
  }
}

void PackScene(PackingScene & scene, const PackingPlan & plan) {  
  scene.dx = 0.3;
  scene.bg.SetMeshPtr(&scene.container.mesh);
  scene.bg.Voxelize(scene.dx);  
  InvertContainer( scene.bg.vox, 1);

  Vec3f dxVec(scene.dx);  
  const unsigned NUM_COPIES = 1000;
  const float outputScale = 1.05f;
  const unsigned ANGLE_TRIALS = 5;

  scene.packFile = scene.outputFolder + "/pack";
  Vec3f voxRes(scene.dx);
  Vec3f origin = scene.bg.GetOrigin();
  scene.placed.resize(scene.items.size());

  scene.trajFile = scene.outputFolder + "/traj";
  for(size_t i = 0;i<plan.steps.size(); i++){
    PackStep(scene, plan.steps[i]);
  } 
}

void PackFruits(const PackingPlan & plan, const std::string & dataDir) {  
  std::string meshDir = dataDir + "fruits_1/";
  PackingScene scene;
  std::vector<MeshInfo> fruits = LoadAllMeshInfo(meshDir);
  scene.items = fruits;
  fs::path containerFile(dataDir + "hands/finger4.8m.stl");
  fs::path innerContainerFile(dataDir + "hands/finger_inner4.8m.stl");
  LoadMeshInfo(scene.container, containerFile);  
  LoadMeshInfo(scene.containerInner, innerContainerFile);
  float broadPhaseDx = 2.0f;
  Box3f containerBox = scene.container.box;
  scene.broadPhase.Init(containerBox ,broadPhaseDx);

  scene.outputFolder = dataDir + "/out/";
  // std::string packedFile = dataDir + "pack0.txt";
  //LoadPack(scene, packedFile);
  //for(unsigned i = 0;i<scene.items.size();i++){
    //SavePackedMesh(scene, i);
  //}
  scene.InitDataStructures();
  PackScene(scene, plan);
}

struct MeshStat{

  std::string type;
  std::string name;

  Box3f box;

  std::string toString() const{
    std::ostringstream oss;
    oss<<name;
    oss << " box " << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << " " << box.vmax[0] << " "
        << box.vmax[1] << " " << box.vmax[2];      
    Vec3f boxSize = box.vmax - box.vmin;
    oss << " size " << boxSize[0] <<" "<<boxSize[1]<<" "<< boxSize[2] ;
    return oss.str();
  }

  void Parse(std::istream & in) {
    in >> name;
    std::string token;
    in >> token;
    in >> box.vmin[0] >> box.vmin[1] >> box.vmin[2] >> box.vmax[0] 
        >> box.vmax[1] >> box.vmax[2];
    // size can be derived from box, so it's not stored in struct.
    // it's stored in file for human readability
    in >> token;
    Vec3f size;
    in>>size[0] >>size[1] >> size[2];
  }
};

void ComputeMeshStats(const std::string & meshDir){
  fs::path inPath(meshDir);
  std::string statsFile = meshDir + "/stats.txt";
  std::ofstream out (statsFile);

  if (!fs::exists(inPath) || !fs::is_directory(inPath)) {
    std::cout << inPath.string() << " not a valid directory\n";
    return;
  }
  for (const auto &entry : fs::directory_iterator(inPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    fs::path p = entry.path();
    std::cout << "Full Filename: " << p.filename() << "\n";
    std::cout << "Stem (Name):   " << p.stem() << "\n";
    std::cout << "Extension:     " << p.extension() << "\n";

    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    if(ext != ".obj" && ext != ".stl"){
      std::cout <<"unknown extension " << ext <<" skip\n";
      continue;
    }

    std::string stem = p.stem().string();
    TrigMesh mesh;
    int ret = LoadMesh(mesh, p);
    MeshStat stat;
    stat.name = p.stem().string();
    stat.box = ComputeBBox(mesh.v);
    out << stat.toString() <<"\n";
  }
  out.close();
}

unsigned GetGroupIndex(float len, const std::vector<float> & thresh){
  for(unsigned i = 0;i<thresh.size();i++){
    if(len > thresh[i]){
      return i;
    }
  }
  return unsigned(thresh.size());
}

PackingPlan PlanPackingSteps(const std::string & meshDir){
  std::vector<MeshStat> stats;
  std::string line;
  std::ifstream statsFile (meshDir + "/stats.txt");
  const unsigned MIN_STAT_LEN = 3;
  while(std::getline(statsFile, line)){
    if(line.size()<MIN_STAT_LEN){
      continue;
    }
    MeshStat stat;
    std::istringstream iss(line);
    stat.Parse(iss);
    stats.push_back(stat);
  }
  std::cout << "loaded " << stats.size() <<" stats\n";
  std::cout <<"last one is " << stats.back().name <<"\n";
  // >20 large, medium large, medium small, small
  std::vector<float> SIZE_THRESH = {20, 10, 5};

  PackingPlan plan;

  plan.groups.resize(SIZE_THRESH.size() + 1);
  std::vector<std::pair<float, size_t>> lenIndex(stats.size());
  for(size_t i = 0;i<stats.size();i++){
    Vec3f boxSize = stats[i].box.vmax - stats[i].box.vmin;
    float len = std::max(std::max(boxSize[0], boxSize[1]), boxSize[2]);
    lenIndex[i] = {len, i};
    unsigned gid = GetGroupIndex(len ,SIZE_THRESH);
    plan.groups[gid].push_back(stats[i].name);
  }

  for(size_t gid = 0; gid < plan.groups.size(); gid++){
    std::vector<std::pair<float, std::string>> lenName;
    for(const auto & name : plan.groups[gid]){
      for(size_t i = 0; i < stats.size(); i++){
        if(stats[i].name == name){
          Vec3f boxSize = stats[i].box.vmax - stats[i].box.vmin;
          float len = std::max(std::max(boxSize[0], boxSize[1]), boxSize[2]);
          lenName.push_back({len, name});
          break;
        }
      }
    }
    std::sort(lenName.begin(), lenName.end(),
              [](const auto& a, const auto& b){ return a.first > b.first; });
    plan.groups[gid].clear();
    for(const auto & p : lenName){
      plan.groups[gid].push_back(p.second);
    }
  }

  // put 20 medium fruits towards the left
  const auto & group = plan.groups[1];
  PackingStep step0;
  step0.names = group;
  step0.count = 20;
  step0.force = Vec3f(-10,0,0);
  plan.steps.push_back(step0);

  // pack as many big then medium fruits as possible towards outside of container.
  const unsigned LARGE_INT = 1000000u;
  for(unsigned g = 0; g<plan.groups.size() - 1; g++){
    PackingStep step;
    step.names = plan.groups[g];
    step.count = LARGE_INT;
    plan.steps.push_back(step);    
  }

  // pack small fruits towards inside of container.
  // add inner container to prevent wasting fruit near center of container.
  PackingStep lastStep;
  lastStep.names = plan.groups.back();
  lastStep.outwards = false;
  lastStep.useInnerContainer = true;
  lastStep.count = 1000000;

  plan.steps.push_back(lastStep);
  return plan;
}

void DebugNudge(){
  PackingScene scene;
  std::string dataDir = "F:/meshes/fruit_hand/";
  std::string meshDir = dataDir + "fruit0/";
  std::vector<MeshInfo> fruits = LoadAllMeshInfo(meshDir);
  scene.items = fruits;
  scene.container;
  fs::path containerFile(dataDir + "box5cm.obj");
  LoadMeshInfo(scene.container, containerFile);  
  float broadPhaseDx = 2.0f;
  Box3f containerBox = scene.container.box;
  scene.broadPhase.Init(containerBox ,broadPhaseDx);
  scene.outputFolder = dataDir + "/out/";
  scene.InitDataStructures();

  Transformation tran;
  tran.position = Vec3f(0,-1.8,-1.8);
  tran.rotation = RotationMatrixRad(0, 0, 0);
  Vec3f pushDir = Vec3f(-1,-1,-1);
  std::vector<Transformation> trajectory;
  Transformation newTran = scene.Nudge(0,tran,pushDir, trajectory);
  unsigned instanceId = scene.Put(0, newTran);
  scene.instances[instanceId].trajectory = trajectory;
  // debug save trajectory progress
  std::string trajFile = dataDir + "traj_debug.txt";
  scene.SaveTrajectories(trajFile);              
}

int main(int argc, char * argv[]){
  // DebugNudge();

  // std::string meshDir = "F:/meshes/fruit_hand/fruits_1/";
  //linux
  std::string dataDir = "/media/desaic/WD/meshes/fruit_hand/";
  std::string meshDir = dataDir + "/fruits_1/";

  // ComputeMeshStats(meshDir);
  auto plan = PlanPackingSteps(meshDir);
  std::cout<<argv[0]<<std::endl;
  PackFruits(plan, dataDir);
  return 0;
}
