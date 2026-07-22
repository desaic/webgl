#pragma once
#include "MeshConvo.h"
class AdapSDF;
struct PackingConstraints;

#include <memory>
// find candidate location for part.
bool FindSpot(MeshConvo &bg,
              const TrigMesh &part,
              Vec3f &pos,
              const Vec3f &rot,
              std::shared_ptr<AdapSDF> sdf,
              float factor = 1.0f);

// find candidate location within a subgrid cell of the container.
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

// find candidate location subject to DOF constraints.
bool FindSpotConstrained(MeshConvo &bg,
                         const TrigMesh &part,
                         Vec3f &pos,
                         const Vec3f &rot,
                         std::shared_ptr<AdapSDF> sdf,
                         float factor,
                         const PackingConstraints &constraints);

