#pragma once

#include "BBox.h"
#include "BroadPhase.h"
#include "RigidTransform.h"
#include "TrigMesh.h"
#include "MeshConvo.h"
#include "MeshInfo.h"
#include "PointSample.h"
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

// why a NudgeToTarget settle loop stopped. Distinguishes "reached the
// target" from "gave up partway" so callers can retry with a fallback
// instead of committing a placement that never actually got there.
enum class NudgeOutcome {
  Arrived,     // came within arrival tolerance of the target.
  Jammed,      // velocity dropped to near zero before arriving (blocked).
  OutOfSteps,  // used the whole step budget, still moving toward target.
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
    Vec3f ForceDirection(
        unsigned itemIdx, const Vec3f &gravity, float gravityWeight, float sdfFactor, const RigidTransform &tran);
    /// @brief compute tighter packing location by moving in a given direction.
    /// @param itemIdx 
    /// @param tran 
    /// @param dirWeight betwee 0-1, weight of dir vs dynamic attraction force.
    /// @return
    RigidTransform Nudge(unsigned itemIdx,
                         const RigidTransform &tran,
                         const Vec3f &dir,
                         float dirWeight,
                         std::vector<RigidTransform> &trajectory);

    RigidTransform NudgeConstrained(unsigned itemIdx, const RigidTransform & tran,
                                    const Vec3f & dir0, const PackingConstraints & constraints,
                                    std::vector<RigidTransform> & trajectory);

    // Nudge toward a fixed world-space target position using a spring force.
    // outcome, if non-null, reports why the settle stopped -- callers should
    // not commit a placement on Jammed/OutOfSteps without checking overlap
    // themselves, since the fruit may have stopped short of the target.
    RigidTransform NudgeToTarget(unsigned itemIdx,
                                 const RigidTransform &tran,
                                 const Vec3f &target,
                                 std::vector<RigidTransform> &trajectory,
                                 NudgeOutcome *outcome = nullptr);

    // Populates items[itemIdx].samples/sdf if not already done, same
    // sampling scheme Nudge/NudgeToTarget generate as a side effect of
    // their own first call for a kind. Exists because a kind resumed from
    // a pack file (LoadPack) via Put() never goes through Nudge/
    // NudgeToTarget at all if nothing of that kind gets freshly placed
    // later in the same run -- its samples/sdf then stay null for the
    // whole process, and every overlap check that depends on them
    // (TryFillSpot's SignedOverlapFraction/ExistingInsideCandidateFraction,
    // PackStep's own post-settle check) silently no-ops against every
    // instance of that kind. Call once per kind right after a resume load,
    // before running anything that places new items, so no existing
    // instance is invisible to overlap checking just because it happened
    // to arrive via a pack file instead of this run's own placement.
    void EnsureItemSamples(unsigned itemIdx);

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


struct CreviceSurface {
    Vec3f pos;
    Vec3f inwardNormal;
    float rayDepth = 0.0f;
};

// Raycast from each surface point along its inward normal to measure gap depth.
// Returns points where rayDepth >= minDepth, Poisson-disk downsampled to exclusionDist.
std::vector<CreviceSurface> FindDeepCreviceSurface(
    const std::vector<SamplePoint> &surfacePoints,
    const TrigGrid &containerGrid,
    float maxRayDist,
    float minDepth,
    float exclusionDist);

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
