#pragma once
#include "MeshConvo.h"
class AdapSDF;

#include <memory>
bool AddUsingSDF(MeshConvo &bg,
                 const TrigMesh &part,
                 Vec3f &pos,
                 const Vec3f &rot,
                 std::shared_ptr<AdapSDF> sdf,
                 float factor = 1.0f);
