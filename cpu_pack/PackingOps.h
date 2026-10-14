#pragma once
#include "MeshConvo.h"
class AdapSDF;
struct PackingConstraints;

#include <memory>
// itemsConv (heuristic only): items-only background, already cropped and FFT'd at
// this call's padded grid size. its convolution, not the full bg's (which
// includes the container wall), supplies the neighbor overlap counts.
// overlapHeuristic: instead of the sdf/position score, keep the collision-free
// position whose 6 neighbors in the convolution have the largest overlap count
// (a snug fit against the most bg surface), ties broken by min x, then y, then z.
// part must be pre-rotated by the caller.
// bgFftReady: true if bg.fft already holds the FFT of bg.vox at this call's
// padded grid size (see FindSpotSubgrid's cache) -- skips recomputing it.
// Only ever pass true when the caller can guarantee that, since a stale
// bg.fft silently produces wrong collision results, not an error.
bool FindSpot(MeshConvo &bg,
              const TrigMesh &part,
              Vec3f &pos,
              std::shared_ptr<AdapSDF> sdf,
              float factor = 1.0f,
              bool bgFftReady = false,
              unsigned erodeVoxels = 0,
              bool overlapHeuristic = false,
              MeshConvo *itemsConv = nullptr);

// caches one cropped-background MeshConvo (voxels + FFT) across repeated
// FindSpotSubgrid calls at the same cell, as long as the crop's own voxel
// bounds and scene.bg's occupancy (bgVersion) have not changed since --
// e.g. the MAX_TRIAL_COUNT random-rotation trials FindSpotSubgrid's caller
// runs per cell before moving on, which for a roughly round item recompute
// an identical crop+FFT every single trial otherwise (see plan.txt: this
// was measured as the dominant cost once the container gets dense enough
// that most trials fail). One instance is safe to reuse across every
// item/cell in a single PackStep call -- the key below is what decides
// reuse, not which item asked.
struct SubgridBgCache {
  int cellIdx = -1;
  unsigned bgVersion = 0;
  Vec3i voxMin = Vec3i(0);
  Vec3i voxMax = Vec3i(0);
  MeshConvo tempConv;
  bool valid = false;
  // empty voxel count per subgrid cell, recomputed when bgVersion changes.
  unsigned emptyCacheVersion = ~0u;
  Vec3u emptyCacheNumCells;
  std::vector<unsigned> cellEmptyCounts;
  // cells whose voxels changed since cellEmptyCounts was computed (fruit placed
  // by this step). lets the refresh recount only those instead of the grid.
  std::vector<unsigned> dirtyCells;
  // cropped + FFT'd items-only background (see PackingScene::itemsBg) for the
  // cell above; valid under the same key as tempConv.
  MeshConvo itemsConv;
  bool itemsValid = false;
};

// part must be pre-rotated by the caller.
// bg is not modified (internal copy of cropped region is used).
// cache, if non-null, is checked first and updated on a miss (see
// SubgridBgCache above) -- purely a speed optimization, never changes
// which spot is found.
bool FindSpotSubgrid(MeshConvo &bg,
                     const TrigMesh &part,
                     Vec3f &pos,
                     std::shared_ptr<AdapSDF> sdf,
                     float factor,
                     float cellSize,
                     unsigned cellIdx,
                     Vec3u numCells,
                     SubgridBgCache *cache = nullptr,
                     unsigned erodeVoxels = 0,
                     bool overlapHeuristic = false,
                     MeshConvo *itemsBg = nullptr);

// part must be pre-rotated by the caller.
bool FindSpotConstrained(MeshConvo &bg,
                         const TrigMesh &part,
                         Vec3f &pos,
                         std::shared_ptr<AdapSDF> sdf,
                         float factor,
                         const PackingConstraints &constraints);

