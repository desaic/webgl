#pragma once

#include "Array3D.h"
#include "PackingConfig.h"
#include "PackingScene.h"
#include "RigidTransform.h"

#include <string>

/// benchmark-only correctness checks. never called from the packing path,
/// so it cannot slow down or perturb an actual pack run.
///
/// approximate: samples the item surface and looks the points up in
/// occupancy grids rather than doing exact mesh intersection.

struct ValidationResult {
    // total surface samples tested.
    unsigned numSamples = 0;
    // samples deeper than allowedDepth into the container wall or exterior.
    unsigned outsideCount = 0;
    // samples deeper than allowedDepth into an already placed item.
    unsigned overlapCount = 0;
    // samples falling off the validation grid entirely.
    unsigned offGridCount = 0;
    // worst measured depth in cm beyond the container wall, over all
    // samples, including ones shallower than allowedDepth.
    float maxOutsideDepth = 0.0f;
    // worst measured depth in cm inside another item.
    float maxOverlapDepth = 0.0f;
    // the threshold the counts above were taken against, echoed so a
    // result is interpretable on its own.
    float allowedDepth = 0.0f;

    bool Ok() const {
      return outsideCount == 0 && overlapCount == 0 && offGridCount == 0;
    }
    std::string toString() const;
};

/// holds a container-only occupancy grid plus an accumulated
/// fruit-only occupancy grid, so the two failure modes stay separable.
class PackValidator {
  public:
    /// voxelizes the container independently of scene.bg, so this can be
    /// called at any point in a run without depending on Put ordering.
    /// container shell and everything outside it become 1 (occupied),
    /// the interior becomes 0 (free).
    void Init(const PackingScene &scene, const PackingConfig &cfg);

    /// checks one candidate placement. does not mutate the grids.
    ValidationResult ValidatePlacement(const PackingScene &scene,
                                       unsigned itemIdx,
                                       const RigidTransform &tran);

    /// records a placement so later checks can detect overlap with it.
    void AddPlaced(const PackingScene &scene, unsigned itemIdx,
                   const RigidTransform &tran);

    /// replays every instance already in the scene into the fruit grid.
    /// use after LoadPack to validate against a partially filled container.
    void AddAllPlaced(const PackingScene &scene);

    /// spacing used to sample item surfaces. smaller is stricter and slower.
    float sampleSpacing = 0.3f;

    /// a sample must be deeper than this, in cm, to count as a violation.
    /// three separate error sources stack up at a real contact, so a
    /// nonzero floor here is required rather than merely convenient:
    ///   - Nudge deliberately allows MAX_OVERLAP = 0.2 cm of penetration
    ///     and MovePointsInward pushes contact samples inward by up to that
    ///   - both meshes are voxelized at dx, rounding occupancy outward
    ///   - AddPlaced marks the shell occupied, extending an item by ~1 voxel
    /// measured worst case on a known good pack is 3 voxels, so this sits
    /// one voxel above that. validate_selftest fails if it is set tight
    /// enough to flag placements taken straight from that pack file.
    float allowedDepth = 1.2f;

    /// depth search stops here. only affects reported magnitudes.
    unsigned maxDepthVoxels = 8;

    size_t MemoryBytes() const;

  private:
    bool WorldToGrid(const Vec3f &p, Vec3u &idx) const;
    /// depth of idx inside the occupied region, in voxels: the Chebyshev
    /// radius of the largest all-occupied cube centered on idx. 0 when idx
    /// itself is free. kept in integer voxels so the threshold comparison
    /// cannot turn on a float rounding difference.
    unsigned DepthVoxelsAt(const Array3D8u &vox, const Vec3u &idx) const;
    /// allowedDepth converted to whole voxels, rounded down.
    unsigned AllowedDepthVoxels() const;

    Array3D8u containerVox;
    Array3D8u fruitVox;
    Vec3f origin;
    float dx = 0.3f;
    Vec3u gridSize;
    bool initialized = false;
};
