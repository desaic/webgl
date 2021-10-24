#include "BBox.h"
#include <vector>
#include <algorithm>

void UpdateBBox(const std::vector<float>& verts, BBox& box)
{
  const unsigned dim = 3;
  for (size_t i = 0; i < verts.size(); i+=3) {
    for (unsigned j = 0; j < dim; j++) {
      box.vmin[j] = std::min(box.vmin[j], verts[i + j]);
      box.vmax[j] = std::max(box.vmax[j], verts[i + j]);
    }
  }
}

void ComputeBBox(const std::vector<float>& verts, BBox& box)
{
  const unsigned dim = 3;
  for (size_t i = 0; i < 3; i++) {
    box.vmin[i] = verts[i];
    box.vmax[i] = verts[i];
  }
  UpdateBBox(verts, box);
}
