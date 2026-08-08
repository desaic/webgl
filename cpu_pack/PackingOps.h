#pragma once
#include "MeshConvo.h"
#include "VoxFootprint.h"
class AdapSDF;
struct PackingConstraints;

#include <functional>
#include <memory>

enum class SpotScoreMode { DEFAULT, POCKET };

// scoring plugged into FindSpot's inner scan loop. DEFAULT reproduces
// today's behavior (sdf distance plus a fixed corner pull) so no existing
// step changes; POCKET lets the pocket search (heuristic_plan.md H2)
// maximize open rays closed instead of proximity to a corner.
struct SpotScore {
  SpotScoreMode mode = SpotScoreMode::DEFAULT;

  // DEFAULT: score = factor * sdf->GetCoarseDist(coord) + positionWeight *
  // (coord.x + coord.y + coord.z), coord being the candidate's mesh-center
  // world position -- exactly today's FindSpot formula.
  std::shared_ptr<AdapSDF> sdf;
  float factor = 1.0f;
  float positionWeight = -1.0f;

  // POCKET: scorer(disp, coord) returns one comparable score for a free
  // candidate. disp is the translation FindSpot would return as pos; coord
  // is disp plus the mesh's local bbox center (DEFAULT's convention). The
  // caller folds any tie-break into this single float, since FindSpot's
  // scan only ever tracks one running best.
  std::function<float(const Vec3f &disp, const Vec3f &coord)> scorer;
};

// find candidate location for part.
bool FindSpot(MeshConvo &bg,
              const TrigMesh &part,
              Vec3f &pos,
              const Vec3f &rot,
              const SpotScore &score);

// convenience overload: today's default scoring.
bool FindSpot(MeshConvo &bg,
              const TrigMesh &part,
              Vec3f &pos,
              const Vec3f &rot,
              std::shared_ptr<AdapSDF> sdf,
              float factor = 1.0f);

// find candidate location within an explicit search box. bg is not
// modified (internal copy of the cropped region is used). shrink is the
// flat amount (cm) sub-5cm items are shrunk by before the FFT search --
// 0.5 matches FindSpotSubgrid's historical behavior; the pocket path passes
// a smaller value (or 0), since a halved berry reports a fit the real berry
// may not have.
bool FindSpotBox(MeshConvo &bg,
                 const TrigMesh &part,
                 Vec3f &pos,
                 const Vec3f &rot,
                 const SpotScore &score,
                 const Box3f &searchBox,
                 float shrink = 0.5f,
                 Vec3f *outCenter = nullptr);

// find candidate location within a subgrid cell of the container. a thin
// wrapper over FindSpotBox: computes the cell's box, searches it with
// today's default scoring and 0.5 cm shrink, and (unless
// ignoreCellBoundary) additionally requires the settled center to land
// back inside the cell -- unchanged from before the FindSpotBox split.
// bg is not modified (internal copy of cropped region is used).
bool FindSpotSubgrid(MeshConvo &bg,
                     const TrigMesh &part,
                     Vec3f &pos,
                     const Vec3f &rot,
                     std::shared_ptr<AdapSDF> sdf,
                     float factor,
                     float cellSize,
                     unsigned cellIdx,
                     Vec3u numCells,
                     bool ignoreCellBoundary = false);

// direct voxel overlap search over the dx lattice within searchBox, for
// boxes small enough that an exhaustive test beats an FFT (heuristic_plan.md
// H2's pocket search): footprint is placed at each lattice position, its
// occupied voxels are read straight out of bg.vox with an early-out on the
// first blocked one, and score.scorer (POCKET mode only -- there is no sdf
// lookup here) ranks every free position rather than FindSpot's implicit
// argmax over a collision map. No shrink, no container-boundary check:
// bg.vox already reads 1 outside the container (InvertContainer), so a
// footprint that pokes out is already blocked by the overlap test itself.
bool FindSpotLocal(MeshConvo &bg,
                   const VoxFootprint &footprint,
                   const Box3f &searchBox,
                   const SpotScore &score,
                   Vec3f &pos);

// find candidate location subject to DOF constraints.
bool FindSpotConstrained(MeshConvo &bg,
                         const TrigMesh &part,
                         Vec3f &pos,
                         const Vec3f &rot,
                         std::shared_ptr<AdapSDF> sdf,
                         float factor,
                         const PackingConstraints &constraints);
