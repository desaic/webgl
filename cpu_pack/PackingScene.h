#pragma once

#include "BBox.h"
#include "BroadPhase.h"
#include "RigidTransform.h"
#include "TrigMesh.h"
#include "MeshConvo.h"
#include "MeshInfo.h"
#include "TrigGrid.h"

#include <string>
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <map>
#include <memory>
#include <unordered_map>

class AdapSDF;

struct InstanceInfo{
  unsigned itemId = 0;
  RigidTransform tran;
  InstanceInfo(unsigned i, const RigidTransform & t):itemId(i), tran(t){}

  // for debug visualization.
  std::vector<RigidTransform> trajectory;
};

struct PackingConstraints {
  // locks the part's x position to fixedPosX.
  bool lockPosX = false;
  float fixedPosX = 0.0f;
  // locks rotation to x-axis only (yz rotation projected out).
  bool lockRotYZ = false;
  // -1: part must stay at x < 0 (left), 0: no constraint, +1: x > 0 (right).
  int xSign = 0;
};

// contact state at the end of a Nudge call, for callers that need to know
// whether the settle actually found real support (heuristic_plan.md H3): a
// part resting only on the container is floating relative to every other
// placed part, which matters once the container itself is not the final
// physical object.
struct NudgeResult {
  unsigned contactCount = 0;
  unsigned itemContactCount = 0; // subset of contactCount that are with other items, not the container.
};

class PackingScene {
  public:

    /// calls InitContainerGrids and ComputeSDF, initializes object vertex normals.
    void InitDataStructures();
    void InitContainerGrids();
    /// transform packing meshes into inertia frame
    void InitRigidBodies();
    void ComputeContainerSDF();
    /// @brief put a copy of itemIdx at the given transformation
    /// revoxelizes because it's not a bottleneck.
    /// @param itemIdx 
    /// @param tran 
    /// @return instance index
    unsigned Put(unsigned itemIdx, const RigidTransform &tran);

    /// heuristic force direction
    Vec3f ForceDirection(unsigned itemIdx, const Vec3f & gravity, float sdfFactor, const RigidTransform & tran);
    /// @brief compute tighter packing location by moving in a given direction.
    /// @param itemIdx 
    /// @param tran 
    /// @return 
    RigidTransform Nudge(unsigned itemIdx, const RigidTransform & tran, const Vec3f & dir, std::vector<RigidTransform> & trajectory, NudgeResult *out = nullptr);

    RigidTransform NudgeConstrained(unsigned itemIdx, const RigidTransform & tran,
                                    const Vec3f & dir0, const PackingConstraints & constraints,
                                    std::vector<RigidTransform> & trajectory);

    Vec3f WorldOrigin()const{
      return bg.GetOrigin();
    }

    unsigned GetItemIndex(const std::string & name) const {
      auto it = nameToIndex.find(name);
      if(it == nameToIndex.end()){
        return 0;
      }
      return it->second;      
    }

    // in both export functions, transformation is converted from inertia frame back to 
    // original input frame.
    void SaveTrajectories(const std::string &filename) const;
    void SaveInstances(const std::string & packFile)const;

    MeshInfo container;
    MeshInfo containerInner;
    bool innerContainerEnabled = false;
    std::vector<MeshInfo> items;
    // for each item, list of transformations
    std::vector<std::vector<RigidTransform> > placed;
    // duplicated with placed.
    std::vector<InstanceInfo>instances;
    
    // mesh name to index into items vector.
    std::map<std::string, unsigned> nameToIndex;
    // 2cm
    float containerSDFDx = 2.0f;
    std::shared_ptr<AdapSDF> sdf;
    std::string outputFolder;
    float dx = 1.0f;
    MeshConvo bg;
    // one byte per voxel, same size and origin as bg.vox: what occupies a
    // voxel (VoxClass.h), not just whether it is free. Metadata only, read
    // by SurfaceCoverage; bg.vox stays the source of truth for collision.
    Array3D8u classGrid;
    float gridDx = 1.0f;
    BroadPhaseGrid broadPhase;
    // container acceleration grid for collission.
    TrigGrid containerGrid;
    TrigGrid containerInnerGrid;
    // cache of TrigGrid per item KIND, built once in the item's local frame
    // and shared by every instance of that kind. keyed by item index, so it
    // is bounded by the item catalogue (~100) rather than by the placement
    // count (thousands). queries transform the point into grid local space.
    //
    // the grids hold a non-owning pointer to items[i].mesh, which is assigned
    // once at load and never reallocated, so the grids stay valid for the run.
    std::unordered_map<unsigned, std::shared_ptr<TrigGrid>> kindGrids;

    std::vector<Vec3f> randAngles;
    // subgrid for FindSpot on small items
    float subgridCellSize = 20.0f;
    Vec3u numSubgridCells;
    // progress saving
    std::string trajFile;
    unsigned trajFileIndex = 0;
    std::string packFile;
};


void LoadPack(PackingScene & scene, const std::string & packFile);

/// @brief once added, irreversible.
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

void MovePointsInward(std::vector<SamplePoint> &points, float offset, const std::shared_ptr<AdapSDF> &sdf);
std::vector<SamplePoint> DownsamplePoints(const std::vector<SamplePoint> &points, float minSpacing);
