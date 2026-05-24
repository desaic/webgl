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

    /// calls InitContainerGrids, initializes object vertex normals.
    void InitDataStructures();
    void InitContainerGrids();

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