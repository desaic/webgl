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

// find candidate location subject to DOF constraints.
bool FindSpotConstrained(MeshConvo &bg,
                         const TrigMesh &part,
                         Vec3f &pos,
                         const Vec3f &rot,
                         std::shared_ptr<AdapSDF> sdf,
                         float factor,
                         const PackingConstraints &constraints);

