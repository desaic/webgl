#include "MeshInfo.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

std::string Transformation::toString() const {
  std::ostringstream oss;
  oss << std::setprecision(6);
  oss << "pos " << position[0] << " " << position[1] << " " << position[2] << " rot " << rotation(0, 0) << " "
      << rotation(0, 1) << " " << rotation(0, 2) << " " << rotation(1, 0) << " " << rotation(1, 1) << " "
      << rotation(1, 2) << " " << rotation(2, 0) << " " << rotation(2, 1) << " " << rotation(2, 2) << " scale "
      << scale;
  return oss.str();
}

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
  RigidBody rb(info.mesh);
  info.rb.Set(rb);
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
    std::cout << entry.path().filename() << "\n";
    fs::path p = entry.path();
    std::cout << "Full Filename: " << p.filename() << "\n";
    std::cout << "Stem (Name):   " << p.stem() << "\n";
    std::cout << "Extension:     " << p.extension() << "\n";

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
