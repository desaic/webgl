#pragma once

#include "Array3D.h"
#include "VoxClass.h"

#include <vector>

// H7: volumetric replacement for SurfaceCoverage's ray-marching, which H6
// found structurally blind to any pocket sitting behind the first thing a
// ray hits from the container surface. This scans classGrid directly:
// VOX_FREE voxels are free space, everything else (container, inner
// boundary, items) is a wall. The scan never needs its own near-surface
// shell logic -- AddInnerContainer already stamps the deep interior's free
// voxels as VOX_INNER, so it reads as a wall here too, exactly bounding the
// search to the shell the user asked for.

struct PocketVolumeParams {
  // must match classGrid's voxel size.
  float dx = 0.3f;
  // a peak shallower than this (cm) is not collected as a target at all --
  // shallow terrain, not a pocket, generalizing
  // PocketPlannerParams::deepPocketThreshold from ray depth to pocket width.
  float minTargetDepth = 1.5f;
  // a component with only ~200 big fruit in a whole container starts out as
  // essentially one connected blob (H7's melon measurement: 19248 of 19272
  // free voxels in one component), not many small pockets -- a single
  // target point per component means one bad spot (wrong shape for every
  // kind, or just unlucky) exhausts the *entire* remaining free volume
  // after only a few fails, long before anywhere near it is actually full.
  // Collecting several separated local maxima per component instead means
  // a bad spot only costs that one spot, not the whole blob.
  unsigned maxPeaksPerComponent = 24;
  // peaks within this many voxel-steps (Chebyshev) of an already-kept peak
  // are treated as the same spot -- keeps the list spread across the
  // component instead of clustering around the single global max.
  unsigned peakSeparationVoxels = 3;
};

// one connected component of free (VOX_FREE) voxels, 6-connected.
struct FreeComponent {
  unsigned voxelCount = 0;
  // separated local maxima of "distance to nearest wall" within this
  // component, sorted by dist descending, capped at maxPeaksPerComponent --
  // see PocketVolumeParams::maxPeaksPerComponent for why more than one.
  struct Peak {
    int dist = 0;      // voxel steps (unweighted 6-connected BFS).
    Vec3i voxel;
    Vec3f point;        // voxel's world-space center.
  };
  std::vector<Peak> peaks;

  // convenience aliases for peaks.front() (the deepest peak), empty
  // component sentinel values if peaks is empty.
  Vec3i targetVoxel;
  int targetDist = -1;
  Vec3f targetPoint;
};

struct PocketVolumeField {
  PocketVolumeParams params;
  Vec3f gridOrigin;
  // one entry per classGrid voxel: index into components, or -1 if the
  // voxel is not free.
  Array3D<int> componentId;
  std::vector<FreeComponent> components;
};

// full-grid rebuild: a multi-source BFS distance-from-wall pass (unweighted,
// 6-connected), then a 6-connected flood fill of VOX_FREE into components,
// collecting each component's separated peaks as it is visited. Both passes
// are single linear scans over classGrid, O(voxel count).
PocketVolumeField BuildPocketVolumeField(const Array3D8u &classGrid,
                                         const Vec3f &gridOrigin,
                                         const PocketVolumeParams &params);
