#pragma once

#include "BBox.h"
#include "BroadPhase.h"
#include "TrigMesh.h"
#include "MeshConvo.h"
#include "MeshInfo.h"
#include "TrigGrid.h"

#include <string>
#include <iomanip>
#include <filesystem>

class AdapSDF;

struct InstanceInfo{
  unsigned itemId = 0;
  Transformation tran;
  InstanceInfo(unsigned i, const Transformation & t):itemId(i), tran(t){}

  // for debug visualization.
  std::vector<Transformation> trajectory;
};

class PackingScene {
  public:

    /// calls InitContainerGrids and ComputeSDF, initializes object vertex normals.
    void InitDataStructures();
    void InitContainerGrids();
    void ComputeContainerSDF();
    /// @brief put a copy of itemIdx at the given transformation
    /// revoxelizes because it's not a bottleneck.
    /// @param itemIdx 
    /// @param tran 
    /// @return instance index
    unsigned Put(unsigned itemIdx, const Transformation &tran);

    /// heuristic force direction
    Vec3f ForceDirection(unsigned itemIdx, const Transformation & tran);
    /// @brief compute tighter packing location by moving in a given direction.
    /// @param itemIdx 
    /// @param tran 
    /// @return 
    Transformation Nudge(unsigned itemIdx, const Transformation & tran, const Vec3f & dir, std::vector<Transformation> & trajectory);

    Vec3f WorldOrigin()const{
      return bg.GetOrigin();
    }

    void SaveTrajectories(const std::string &filename) const;

    MeshInfo container;
    MeshInfo containerInner;
    std::vector<MeshInfo> items;
    // for each item, list of transformations
    std::vector<std::vector<Transformation> > placed;
    // duplicated with placed.
    std::vector<InstanceInfo>instances;
    std::vector<int> sortedBySize;
    // 2cm
    float containerSDFDx = 2.0f;
    std::shared_ptr<AdapSDF> sdf;
    std::string outputFolder;
    float dx = 1.0f;
    MeshConvo bg;
    float gridDx = 1.0f;
    BroadPhaseGrid broadPhase;
    // container acceleration grid for collission.
    TrigGrid containerGrid;
    TrigGrid containerInnerGrid;
};


void LoadPack(PackingScene & scene, const std::string & packFile);

void AddInnerContainer(PackingScene & scene);

/// @brief 
/// @param scene 
/// @param i item type index
void SavePackedMesh(const PackingScene &scene, unsigned i);

int LoadMesh(TrigMesh &m, const std::filesystem::path & p) ;

/// @brief computes dense sdf grid for packing.
/// @param distUnit
/// @param h
/// @param mesh
/// @return
std::shared_ptr<AdapSDF> ComputeSDF(float distUnit, float h, TrigMesh &mesh);

/// @brief compute gradient field from sdf using central differences
/// @param sdf
/// @param distUnit
/// @param voxSize
/// @return
Array3D<Vec3f> ComputeSDFGradient(const AdapSDF& sdf, float distUnit, float voxSize);

/// @brief save gradient field as line segments to obj file
/// @param filename
/// @param gradients
/// @param sdf
/// @param voxSize
/// @param stride subsample stride for visualization
void SaveGradientObj(const std::string& filename, const Array3D<Vec3f>& gradients,
                     const AdapSDF& sdf, float voxSize, unsigned stride);