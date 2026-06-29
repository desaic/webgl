#include "MeshInfo.h"
#include "Matrix4f.h"

#include <AdapSDF.h>
#include <AdapDF.h>
#include <FastSweep.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

int LoadMesh(TrigMesh &m, const fs::path & p) {
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(),ext.begin(), [](unsigned char c) { return std::tolower(c); });
  int ret = -1;
  if (ext == ".stl") {
    ret = m.LoadStl(p.string());
  } else if (ext == ".obj") {
    ret = m.LoadObj(p.string());
  } else {
    return -1;
  }
  return ret;
}

int LoadMeshInfo(MeshInfo &info, const fs::path &p) {
  int ret = LoadMesh(info.mesh, p);
  if (ret != 0) {
    return ret;
  }
  info.name = p.stem().string();
  info.box = ComputeBBox(info.mesh.v);  
  info.rb.SetMesh(info.mesh);
  return ret;
}

std::vector<MeshInfo> LoadAllMeshInfo(const std::string &meshDir) {
  std::vector<MeshInfo> fruits;
  fs::path inPath(meshDir);

  if (!fs::exists(inPath) || !fs::is_directory(inPath)) {
    std::cout << inPath.string() << " not a valid directory\n";
    return fruits;
  }
  for (const auto &entry : fs::directory_iterator(inPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    fs::path p = entry.path();
    std::cout << "Filename: " << p.filename() << " | ";
    std::cout << p.stem() << " | ";
    std::cout << p.extension() << "\n";

    std::string stem = p.stem().string();
    if (stem.ends_with("_m")) {
      // skip mirrored meshes.
      continue;
    }
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    if(ext != ".obj" && ext != ".stl"){
      std::cout <<"unknown extension " << ext <<" skip\n";
      continue;
    }
    MeshInfo info;
    int ret = LoadMeshInfo(info, p);    
    if (ret != 0) {
      std::cout << " error " << ret << " loading " << p.filename().string() << "\n";
      continue;
    }
    info.filePath = p.string();
    fruits.push_back(info);
  }

  return fruits;
}

std::vector<int> SortBySize(const std::vector<MeshInfo> &items) {
  std::vector<int> indices(items.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(),
            [&](int i, int j) { return items[i].BoxDiagonal() > items[j].BoxDiagonal(); });
  return indices;
}

void MeshInfo::ComputeSDFCached(){
  if(this->sdf){
    //naive logic for only computing it once.
    return;
  }
  this->sdf = std::make_shared<AdapSDF>();
  const float MIN_H = 0.2f;
  Vec3f boxSize = this->box.vmax - this->box.vmin;
  float maxLen = std::max(std::max(boxSize[0], boxSize[1]), boxSize[2]);
  sdf->voxSize = std::max(maxLen / 128.0f, MIN_H);
  float distUnit = 0.01f * sdf->voxSize;
  sdf->band = 16;
  sdf->distUnit = distUnit;
  sdf->BuildTrigList(&mesh);
  sdf->Compress();
  mesh.ComputePseudoNormals();
  sdf->ComputeCoarseDist();
  //CloseExterior(sdf->dist, sdf->MAX_DIST);
  Array3D8u frozen;
  int band = 8;
  FastSweepPar(sdf->dist, sdf->voxSize, distUnit, band, frozen);
}

