#pragma once
#include "MeshConvo.h"
class AdapSDF;
struct PackingConstraints;

#include <memory>
// part must be pre-rotated by the caller.
bool FindSpot(MeshConvo &bg,
              const TrigMesh &part,
              Vec3f &pos,
              std::shared_ptr<AdapSDF> sdf,
              float factor = 1.0f);

// part must be pre-rotated by the caller.
// bg is not modified (internal copy of cropped region is used).
bool FindSpotSubgrid(MeshConvo &bg,
                     const TrigMesh &part,
                     Vec3f &pos,
                     std::shared_ptr<AdapSDF> sdf,
                     float factor,
                     float cellSize,
                     unsigned cellIdx,
                     Vec3u numCells);

// part must be pre-rotated by the caller.
bool FindSpotConstrained(MeshConvo &bg,
                         const TrigMesh &part,
                         Vec3f &pos,
                         std::shared_ptr<AdapSDF> sdf,
                         float factor,
                         const PackingConstraints &constraints);

