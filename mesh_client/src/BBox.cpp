#include "BBox.h"

#include <algorithm>

void BBox2D::Compute(const std::vector<Vec2f>& verts)
{
  if (verts.size() == 0) {
    return;
  }
  mn = verts[0];
  mx = verts[0];
  const unsigned dim = 2;
  for (size_t i = 1; i < verts.size(); i++) {
    for (unsigned d = 0; d < dim; d++) {
      mx[d] = std::max(verts[i][d], mx[d]);
      mn[d] = std::min(verts[i][d], mn[d]);
    }
  }
}

void UpdateBBox(const std::vector<float>& verts, BBox& box)
{
  const unsigned dim = 3;
  for (size_t i = 0; i < verts.size(); i+=3) {
    for (unsigned j = 0; j < dim; j++) {
      box.mn[j] = std::min(box.mn[j], verts[i + j]);
      box.mx[j] = std::max(box.mx[j], verts[i + j]);
    }
  }
}

void ComputeBBox(const std::vector<float>& verts, BBox& box)
{
  const unsigned dim = 3;
  for (size_t i = 0; i < 3; i++) {
    box.mn[i] = verts[i];
    box.mx[i] = verts[i];
  }
  UpdateBBox(verts, box);
}
