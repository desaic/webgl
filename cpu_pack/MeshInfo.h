#pragma once
#include "BBox.h"
#include "RigidBody.h"
#include "TrigMesh.h"
#include "Matrix3f.h"
#include "Matrix4f.h"

#include <filesystem>
#include <string>

class MeshInfo {
  public:
    Box3f box;
    std::string name;
    std::string filePath;
    TrigMesh mesh;

    RigidBody rb;
    
    bool noMoreFit = false;
    float BoxDiagonal() const {
      return (box.vmax - box.vmin).norm();
    }
};

int LoadMesh(TrigMesh &m, const std::filesystem::path &p);

int LoadMeshInfo(MeshInfo &info, const std::filesystem::path &p);

std::vector<MeshInfo> LoadAllMeshInfo(const std::string &meshDir);

std::vector<int> SortBySize(const std::vector<MeshInfo> &items);
