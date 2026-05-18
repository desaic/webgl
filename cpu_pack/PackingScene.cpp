#include "PackingScene.h"
#include "cpu_voxelizer.h"
#include "MeshConvo.h"
#include "FloodOutside.h"
#include "GridUtils.h"
#include "MeshOps.h"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

Vec3i roundf(const Vec3f & fvec){
  return Vec3i(std::round(fvec[0]),std::round(fvec[1]),std::round(fvec[2]));
}

void PackingScene::Put(unsigned itemIdx, const Transformation &tran){
  TrigMesh inst = MakeTransformedMesh(items[itemIdx].mesh, tran);
  Box3f bbox = ComputeBBox(inst.v);
  // saved for broad phase.
  Box3f meshBox = bbox;
  bbox.vmin = AlignOriginToGrid(bbox.vmin, dx);
    
  VoxConf conf;
  conf.origin = ToArray(bbox.vmin);
  conf.unit = {dx, dx, dx};  
  conf.gridSize = ComputeGridSize(bbox, dx, 1);

  Array3D8u vox;
  vox.Allocate(conf.gridSize, 0);
  VoxelizeMesh(inst, vox, conf);
  FloodOutside8u(vox, 1, 2);

  // e.g. if item origin is same as world origin, then offset is 0.
  Vec3i offset = roundf((1.0f / dx) * (conf.origin - WorldOrigin()));
  Union(bg, offset, vox);

  unsigned instId = placed.size();
  placed[itemIdx].push_back(tran);
  /// @TODO: add item to broarphase grid
  broadPhase.Add(meshBox , instId);
}

Vec3f PackingScene::ForceDirection(unsigned itemIdx, const Transformation & tran)
{
  Vec3f dir (-1,-1, -1);

  // push away or towards center depending on item size group.
  Vec3f center = bg.GetMeshCenter();

  return dir;
}

Transformation PackingScene::Nudge(unsigned itemIdx, const Transformation & tran, const Vec3f & dir){
  Transformation tOut = tran;

  return tOut;
}

void LoadPack(PackingScene & scene, const std::string & packFile){
  std::ifstream in (packFile);
  if (!in.good()) {
    return;
  }

  // Initialize placed vector with empty vectors for each item
  scene.placed.resize(scene.items.size());

  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string itemName, posLabel, rotLabel, scaleLabel;
    Transformation trans;

    // Parse: ItemName pos x y z rot x y z scale s
    if (iss >> itemName >> posLabel
        >> trans.position[0] >> trans.position[1] >> trans.position[2]
        >> rotLabel
        >> trans.rotation[0] >> trans.rotation[1] >> trans.rotation[2]
        >> scaleLabel >> trans.scale) {

      // Find which item index this corresponds to
      int itemIndex = -1;
      for (size_t i = 0; i < scene.items.size(); i++) {
        if (scene.items[i].name == itemName) {
          itemIndex = i;
          break;
        }
      }

      if (itemIndex >= 0) {
        scene.placed[itemIndex].push_back(trans);
      } else {
        std::cout << "Warning: item " << itemName << " not found in scene\n";
      }
    }
  }
  
  // Print summary
  for (size_t i = 0; i < scene.items.size(); i++) {
    if (!scene.placed[i].empty()) {
      std::cout << "Loaded " << scene.placed[i].size() << " placements for "
                << scene.items[i].name << "\n";
    }
  }
}

static std::array<float, 3> ToArray(const Vec3f &v) {
  return {v[0], v[1], v[2]};
}

void AddInnerContainer(PackingScene & scene){
  float dx =scene.dx;
  Box3f box = scene.container.box;
  VoxConf conf;
  conf.origin = ToArray(box.vmin);
  conf.unit = {dx, dx, dx};

  conf.gridSize = ComputeGridSize(box, dx, 1);
  Array3D8u vox;
  vox.Allocate(conf.gridSize, 0);
  
  vox.Allocate(conf.gridSize, 0);
  cpu_voxelize_grid(conf, &scene.containerInner.mesh, vox);
  FloodOutside8u(vox, 1, 2);
  ThreshInPlace(vox, 1);
  Union(scene.bg, Vec3i(0), vox);
}

/// @brief 
/// @param scene 
/// @param i item type index
void SavePackedMesh(const PackingScene &scene, unsigned i) {
  std::string name = scene.items[i].name;
  TrigMesh m = MakeMergedMesh(scene.items[i].mesh, scene.placed[i]);
  if (!m.v.empty()) {
    std::string outMeshName = scene.outputFolder + name + "_merge.obj";
    m.SaveObj(outMeshName);
  }
}
