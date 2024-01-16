#define _USE_MATH_DEFINES
#include <cmath>

#include "MakeHelix.h"
#include  <algorithm>

/// <summary>
/// 
/// </summary>
/// <param name="ymin"></param>
/// <param name="y0"></param>
/// <param name="y1"></param>
/// <param name="r"></param>
/// <param name="divs"></param>
/// <param name="pitch"></param>
/// <param name="startingIdx">index of the first vertex included in the helix</param>
/// <returns></returns>
std::vector<Vec3f> MakeOneHelix(float ymin, float y0, float y1, float r, unsigned divs,
                                float pitch, unsigned & startingIdx) {
  float y = y0;
  float dyPerStep = pitch / divs;
  std::vector<Vec3f> points;
  unsigned vCount = 0;
  bool started = false;
  while (y < y1) {
    float phase = vCount % divs / float(divs);
    float theta = phase * (2 * M_PI);
    float x = r * std::sin(theta);
    float z = r * std::cos(theta);
    y = y0 + dyPerStep * vCount;
    if (y >= ymin) {
      if (!started) {
        startingIdx = vCount;
        started = true;
      }
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

struct VertCol {
  bool hasVert[4] = {};
  Vec3f verts[4];
};

std::vector<VertCol> GroupIntoCols(const std::vector<Vec3f>& v0, const std::vector<Vec3f>& v1,
                   const std::vector<Vec3f>& v2, const std::vector<Vec3f>& v3,
  const unsigned * startIdx) {
  size_t numCols = startIdx[3] + v3.size();
  std::vector<VertCol> cols;
  for (size_t i = 0; i < numCols; i++) {
    VertCol col;
    cols.push_back(col);
  }
  return cols;
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
  unsigned startIdx[4] = {};
  std::vector<Vec3f> h0 = MakeOneHelix(ymin, y0, y1, r0, spec.divs, spec.pitch, startIdx[0]);
  y0 = -spec.inner_width + slopeWidth;
  std::vector<Vec3f> h1 =
      MakeOneHelix(ymin, y0, y1, r1, spec.divs, spec.pitch, startIdx[1]);
  y0 = -slopeWidth;
  std::vector<Vec3f> h2 =
      MakeOneHelix(ymin, y0, y1, r1, spec.divs, spec.pitch, startIdx[2]);
  y0 = 0;
  std::vector<Vec3f> h3 =
      MakeOneHelix(ymin, y0, y1, r0, spec.divs, spec.pitch, startIdx[3]);
  AddVerts(h0, mesh);
  AddVerts(h1, mesh);
  AddVerts(h2, mesh);
  AddVerts(h3, mesh);

  std::vector<VertCol> cols = GroupIntoCols(h0, h1, h2, h3, startIdx);

  return 0;
}
