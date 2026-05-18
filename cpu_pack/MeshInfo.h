#pragma once
#include "BBox.h"
#include "TrigMesh.h"

#include <filesystem>
#include <string>

class MeshInfo {
  public:
    Box3f box;
    std::string name;
    TrigMesh mesh;
    // large medium small.
    unsigned groupId=0;
    bool noMoreFit = false;
    float BoxDiagonal() const {
      return (box.vmax - box.vmin).norm();
    }
};

// Structure to store transformation data
struct Transformation {
    Vec3f position;
    Vec3f rotation;
    float scale = 1.0f;
    std::string toString() const;
};

int LoadMesh(TrigMesh &m, const std::filesystem::path &p);

int LoadMeshInfo(MeshInfo &info, const std::filesystem::path &p);

std::vector<MeshInfo> LoadAllMeshInfo(const std::string &meshDir);

std::vector<int> SortBySize(const std::vector<MeshInfo> &items);
