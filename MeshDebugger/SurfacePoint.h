#pragma once

#include "Vec2.h"
#include "Vec3.h"

struct SurfacePoint {
  Vec3f v;
  Vec3f n;
  Vec2f uv;
  // tangent vectors using uv parameterization
  // dx/du and dx/dv where x is surface point.
  Vec3f tx, ty;
};
