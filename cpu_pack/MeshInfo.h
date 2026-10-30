#pragma once
#include "BBox.h"
#include "RigidBody.h"
#include "TrigMesh.h"
#include "Matrix3f.h"
#include "Matrix4f.h"
#include "PointSample.h"

#include <filesystem>
#include <string>
#include <memory>

class AdapSDF;

class MeshInfo {
  public:
    /// false for a stub made by a lazy load (name and filePath only, empty mesh).
    bool IsLoaded() const { return !mesh.v.empty(); }
    Box3f box;
    std::string name;
    std::string filePath;
    TrigMesh mesh;

    RigidBody rb;
    std::shared_ptr<AdapSDF> sdf;
    std::vector<SamplePoint> samples;
    bool noMoreFit = false;
    unsigned nextCellIdx = 0;
    float BoxDiagonal() const {
      return (box.vmax - box.vmin).norm();
    }
    // sets sdf pointer to a new sdf instance
    void ComputeSDFCached();
};

int LoadMesh(TrigMesh &m, const std::filesystem::path &p);

int LoadMeshInfo(MeshInfo &info, const std::filesystem::path &p);

/// lazy: only registers name and filePath per mesh, leaving the mesh empty.
/// PackingScene::EnsureItemLoaded loads one on first use.
std::vector<MeshInfo> LoadAllMeshInfo(const std::string &meshDir, bool lazy = false);

std::vector<int> SortBySize(const std::vector<MeshInfo> &items);
