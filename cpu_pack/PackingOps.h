#pragma once
#include "MeshConvo.h"
class AdapSDF;
struct PackingConstraints;

#include <memory>
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
              bool bgFftReady = false);

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
                     SubgridBgCache *cache = nullptr);

// part must be pre-rotated by the caller.
bool FindSpotConstrained(MeshConvo &bg,
                         const TrigMesh &part,
                         Vec3f &pos,
                         std::shared_ptr<AdapSDF> sdf,
                         float factor,
                         const PackingConstraints &constraints);

