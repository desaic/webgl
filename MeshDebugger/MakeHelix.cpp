#define _USE_MATH_DEFINES
#include <cmath>

#include "MakeHelix.h"
#include  <algorithm>

std::vector<Vec3f> MakeOneHelix(float ymin, float y0, float y1, float r, unsigned divs,
                                float pitch) {
  float y = y0;
  float dyPerStep = pitch / divs;
  std::vector<Vec3f> points;
  unsigned vCount = 0;
  while (y < y1) {
    float phase = vCount % divs / float(divs);
    float theta = phase * (2 * M_PI);
    float x = r * std::sin(theta);
    float z = r * std::cos(theta);
    y = y0 + dyPerStep * vCount;
    if (y >= ymin) {
      // only add verts above ground
      points.push_back(Vec3f(x, y, z));
    }
    vCount++;
  }
  return points;
}

void AddVerts(const std::vector<Vec3f>& vec, TrigMesh& mesh) {
  for (const auto v : vec) {
    mesh.AddVert(v[0], v[1], v[2]);
  }
}

//   y      v3
//  /|\    \  v2
//   |      | 
//   |     /  v1
//   |     v0
int MakeHelix(const HelixSpec& spec, TrigMesh& mesh) {
  unsigned MAX_DIVS_PER_REV = 180;
  mesh = MakeCube(Vec3f(0, 0, 0), Vec3f(1, 1, 1));
  //helix is cut off from below this value
  float ymin = 0;
  float y1 = spec.length;
  unsigned numV0 = 0, numV1 = 0, numV2 = 0, numV3 = 0;
  //v3 starts at y = 0.
  float y0 = -spec.inner_width;
  unsigned vCount = 0;
  unsigned divs = std::min(spec.divs, MAX_DIVS_PER_REV);
  float r0 = spec.inner_diam / 2;
  float r1 = spec.outer_diam / 2;
  float slopeWidth = std::abs(spec.inner_width - spec.outer_width) / 2;
  std::vector<Vec3f> h0 = MakeOneHelix(ymin, y0, y1, r0, spec.divs, spec.pitch);
  y0 = -spec.inner_width + slopeWidth;
  std::vector<Vec3f> h1 = MakeOneHelix(ymin, y0, y1, r1, spec.divs, spec.pitch);
  y0 = -slopeWidth;
  std::vector<Vec3f> h2 = MakeOneHelix(ymin, y0, y1, r1, spec.divs, spec.pitch);
  y0 = 0;
  std::vector<Vec3f> h3 = MakeOneHelix(ymin, y0, y1, r0, spec.divs, spec.pitch);
  AddVerts(h0, mesh);
  AddVerts(h1, mesh);
  AddVerts(h2, mesh);
  AddVerts(h3, mesh);

  return 0;
}
