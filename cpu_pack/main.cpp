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
#include "PackDebugScene.h"

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

struct PackingStep {

    std::vector<std::string> names;

    Vec3f force;

    unsigned count = 0;
    // pack towards inside or outside of container.
    bool outwards = true;
    // prevent packing at center of container
    bool useInnerContainer = false;

    PackingStep() : force(-1, 0, 0) {}

    std::string toString() const {
      std::ostringstream oss;
      oss << "names " << names.size() << " ";
      for (size_t i = 0; i < names.size(); i++) {
        oss << names[i] << " ";
      }
      oss << " force " << force[0] << " " << force[1] << " " << force[2] << " ";
      oss << "count " << count;
      oss << " outwards " << outwards << " useInnerContainer " << useInnerContainer;
      return oss.str();
    }

    void Parse(std::istream &in) {}
};

struct PackingPlan{
  std::vector<PackingStep> steps;
  // lists of meshes grouped by sizes.
  std::vector< std::vector<std::string> > groups;

  void Save(std::ostream & out){
    out << "groups " << groups.size() <<"\n";
    for(size_t i = 0;i< groups.size();i++){
      out << groups[i].size() << " ";
      for(size_t j = 0;j<groups[i].size() ;j++){
        out << groups[i][j] << " ";
      }
      out <<"\n";
    }
    out << "num_steps " << steps.size() << "\n";
    for(size_t i = 0;i<steps.size();i++){
      out << steps[i].toString () <<"\n";
    }
  }

  void Load(std::istream & in){

  }
};

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

void PackScene(PackingScene & scene) {  
  scene.dx = 0.3;
  scene.bg.SetMeshPtr(&scene.container.mesh);
  scene.bg.Voxelize(scene.dx);  
  InvertContainer( scene.bg.vox, 1);

  Vec3f dxVec(scene.dx);  
  const unsigned NUM_COPIES = 1000;
  const float outputScale = 1.05f;
  const unsigned ANGLE_TRIALS = 5;
  //euler angles in radians.
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
    unsigned itemIndex = scene.sortedBySize[i];
    itemGroups[groupIndex].push_back(itemIndex);
    scene.items[itemIndex].groupId = groupIndex;
  }

  unsigned angleIndex = 0;
  const unsigned MAX_TRIAL_COUNT = 10;

  unsigned debugCount = 0;

  std::string ProgressFileName = scene.outputFolder + "/traj_progress";

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
        std::string itemName = scene.items[itemIndex].name;
        bool itemPlaced = false;
        for(unsigned trial = 0; trial<MAX_TRIAL_COUNT; trial++){
          Vec3f pos;
          Vec3f rot = randAngles[angleIndex];
          angleIndex ++;                
          if(angleIndex >= randAngles.size()){
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

            Vec3f pushDir = scene.ForceDirection(itemIndex, tran);
            std::vector<Transformation> trajectory;
            Transformation newTran = scene.Nudge(itemIndex,tran,pushDir, trajectory);
            unsigned instanceId = scene.Put(itemIndex, newTran);
            scene.instances[instanceId].trajectory = trajectory;
            // debug
            debugCount ++;
            if(debugCount % 10 == 0 && debugCount > 0){
              std::string debugProgressFile = ProgressFileName + std::to_string(int(debugCount/10) % 10) + ".txt";
              scene.SaveTrajectories(debugProgressFile);              
            }
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
  }
}

void PackFruits() {
  std::string dataDir = "F:/meshes/fruit_hand/";
  std::string meshDir = dataDir + "fruits/";
  PackingScene scene;
  std::vector<MeshInfo> fruits = LoadAllMeshInfo(meshDir);
  scene.items = fruits;
  scene.container;
  fs::path containerFile(dataDir + "hands/finger4.8m.stl");
  // fs::path containerFile(dataDir + "box5cm.obj");
  fs::path innerContainerFile(dataDir + "hands/finger_inner4.8m.stl");
  LoadMeshInfo(scene.container, containerFile);  
  LoadMeshInfo(scene.containerInner, innerContainerFile);
  float broadPhaseDx = 2.0f;
  Box3f containerBox = scene.container.box;
  scene.broadPhase.Init(containerBox ,broadPhaseDx);
  scene.sortedBySize = SortBySize(scene.items);

  std::cout << "computing container sdf done \n";
  scene.outputFolder = dataDir + "/out/";
  std::string packedFile = dataDir + "pack0.txt";
  //LoadPack(scene, packedFile);
  //for(unsigned i = 0;i<scene.items.size();i++){
    //SavePackedMesh(scene, i);
  //}
  scene.InitDataStructures();
  PackScene(scene);
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
  for(size_t i = 0;i<stats.size();i++){
    Vec3f boxSize = stats[i].box.vmax - stats[i].box.vmin;
    float len = std::max(std::max(boxSize[0], boxSize[1]), boxSize[2]);
    unsigned gid = GetGroupIndex(len ,SIZE_THRESH);
    plan.groups[gid].push_back(stats[i].name);
  }

  // put 20 medium fruits towards the left
  const auto & group = plan.groups[1];
  PackingStep step;
  step.names = group;
  step.count = 20;
  plan.steps.push_back(step);
  return plan;
}

int main(int argc, char * argv[]){
  std::string meshDir = "F:/meshes/fruit_hand/fruits_1/";
  ComputeMeshStats(meshDir);
  auto plan = PlanPackingSteps(meshDir);
  std::ofstream planOut(meshDir + "plan.txt");
  plan.Save(planOut);
  planOut.close();

  std::cout<<argv[0]<<std::endl;
 // PackFruits();  
  // PackDebug();
  return 0;
}
