#pragma once
#include "MeshConvo.h"
class AdapSDF;
struct PackingConstraints;

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
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
// contact score used instead of the sdf/position score when overlapHeuristic is on and enabled.
// among collision-free positions it sums, over the 6 axis shifts of the eroded fruit, the overlap
// with the items-only background (voxel faces that would touch), i.e. the contact area. a position
// needs contact in at least minDirs of the 6 directions (each with at least minFace voxels), so
// a fruit lying against one flat side does not win but one in a groove between two surfaces does.
// score = contact area (cm2) + wallW * distance to the container wall (cm, capped at wallCap)
// - posW * (x + y + z), the last as a tie breaker only.
// one scored free position, the center position of the part (same convention as FindSpot's pos).
struct SpotCand {
  Vec3f pos;
  float score;
};

struct SpotScoreParams {
  bool enabled = false;
  float wallW = 0.5f;
  float wallCap = 10.0f;
  float posW = 0.01f;
  unsigned minDirs = 2;
  unsigned minFace = 4;
  // enclosure gate: fraction of a shell (shellCm thick) around the fruit occupied by fruit (items-only
  // background, the container wall is ignored) must reach minBuried. a fruit resting on one convex surface
  // blocks about a third of its shell or less, one in a dimple or groove blocks well over half.
  float shellCm = 2.0f;
  float minBuried = 0.6f;
  float buriedW = 10.0f;   // score += buriedW * buried fraction
  // the subgrid search shrinks a fruit under 5 cm by this much (cm, whole extent) before the FFT so it fits
  // tight pockets; 0 keeps the true size, so a spot the search calls free is free for the real fruit.
  float shrinkCm = 0.5f;
};

// voxelization of a rotated fruit, after thresholding and erosion and before the FFT reversal.
// the same (kind, rotation) recurs for every cell, so FindSpot keeps these by mesh hash.
struct FgCacheEntry {
  Array3D8u vox;
  Box3f box;
  Box3f meshBox;
};
using FgCache = std::unordered_map<uint64_t, FgCacheEntry>;

// shrinks the occupied voxels by n (6-neighborhood), the same erosion the spot search applies to the fruit.
void ErodeVox(Array3D8u &vox, unsigned n);

// fgCache (optional): voxelization cache, see FgCacheEntry.
// itemsProvider (optional, heuristic only): returns the items-only background when first
// needed. FindSpot returns before calling it if the trial has no collision-free position, so
// the crop and FFT behind it are only paid for in cells that can actually take the fruit.
bool FindSpot(MeshConvo &bg,
              const TrigMesh &part,
              Vec3f &pos,
              std::shared_ptr<AdapSDF> sdf,
              float factor = 1.0f,
              bool bgFftReady = false,
              unsigned erodeVoxels = 0,
              bool overlapHeuristic = false,
              MeshConvo *itemsConv = nullptr,
              FgCache *fgCache = nullptr,
              const std::function<MeshConvo *()> *itemsProvider = nullptr,
              const SpotScoreParams *spotParams = nullptr,
              float *outScore = nullptr,
              std::vector<SpotCand> *outCands = nullptr,
              float nmsCm = 0.0f,
              unsigned maxCands = 0);

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
  FgCache fgCache;
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
                     MeshConvo *itemsBg = nullptr,
                     const SpotScoreParams *spotParams = nullptr,
                     float *outScore = nullptr,
                     std::vector<SpotCand> *outCands = nullptr,
                     float nmsCm = 0.0f,
                     unsigned maxCands = 0);

// part must be pre-rotated by the caller.
bool FindSpotConstrained(MeshConvo &bg,
                         const TrigMesh &part,
                         Vec3f &pos,
                         std::shared_ptr<AdapSDF> sdf,
                         float factor,
                         const PackingConstraints &constraints);

