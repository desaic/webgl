#pragma once
#include "BBox.h"
#include "TrigMesh.h"
#include "Matrix3f.h"
#include <filesystem>
#include <string>

class MeshInfo {
  public:
    Box3f box;
    std::string name;
    std::string filePath;
    TrigMesh mesh;

    bool noMoreFit = false;
    float BoxDiagonal() const {
      return (box.vmax - box.vmin).norm();
    }
};

// Structure to store transformation data
struct Transformation {
    Vec3f position;
    Matrix3f rotation;    
    float scale = 1.0f;
    Transformation():rotation(Matrix3f::identity()){}
    Transformation(const Vec3f & pos, const Matrix3f & rot):position(pos),
    rotation(rot){}
    /// @brief 
    /// @return rotation matrix is printed in row major order. even though internally stored in column major order.
    std::string toString() const;
};

int LoadMesh(TrigMesh &m, const std::filesystem::path &p);

int LoadMeshInfo(MeshInfo &info, const std::filesystem::path &p);

std::vector<MeshInfo> LoadAllMeshInfo(const std::string &meshDir);

std::vector<int> SortBySize(const std::vector<MeshInfo> &items);
