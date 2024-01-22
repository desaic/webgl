#define _USE_MATH_DEFINES
#include <cmath>

#include "cgal_wrapper.h"

#include "MakeHelix.h"
#include "MeshUtil.h"
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

//assume start0 > start1
void FillRow(const std::vector<Vec3f>& h0, const std::vector<Vec3f>& h1, int start0,
        int start1, TrigMesh & mesh, float ymax) {
  unsigned maxCol = start0 + h0.size();
  if (h0.size() < 2 || h1.size() <2) {
    return;
  }

  unsigned vCountOld = mesh.GetNumVerts();
  unsigned vCount = vCountOld + h0.size() + h1.size();
  AddVerts(h0, mesh);
  AddVerts(h1, mesh);
  //radius in xz plane
  float r0 = std::sqrt(h0[0][0] * h0[0][0] + h0[0][2] * h0[0][2]);
  float r1 = std::sqrt(h1[0][0] * h1[0][0] + h1[0][2] * h1[0][2]);
  float dyPerStep = h1[1][1] - h1[0][1];
  //number of added vertices at y=0.
  unsigned y0Count = 0;
  // at ymax
  unsigned ymaxCount = 0;
  //create missing vertices
  for (unsigned col = start1; col < maxCol; col++) {
    unsigned i1 = col - start1;
    if (col < start0) {
      float y0 = -h0[0][1] + (start0 - col) * dyPerStep;
      float y1 = h1[i1][1];
      float radIntersection = r1 + y0 / (y0 + y1) * (r0 - r1);
      //make a new vertex connecting h1[i1] to y=0.
      unsigned v0Idx = vCount;
      Vec3f v0 = (radIntersection / r1) * h1[i1];
      v0[1] = 0;
      vCount++;
      mesh.AddVert(v0[0], v0[1], v0[2]);
      y0Count++;
    }
  }
  for (unsigned col = start1; col < maxCol; col++) {
    if (col >= start1 + h1.size()) {
      unsigned i0 = col - start0;
      float y0 = ymax - h0[i0][1];
      float y1 = h1[0][1] + (col - start1) * dyPerStep - ymax;
      float radIntersection = r1 + y0 / (y0 + y1) * r0;
      // make a new vertex connecting h0[i0] to y=0.
      unsigned v0Idx = vCount;
      Vec3f v0 = (radIntersection / r1) * h0[i0];
      vCount++;
      mesh.AddVert(v0[0], v0[1], v0[2]);
      ymaxCount++;
    }
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
  unsigned startIdx[4] = {};
  std::vector<Vec3f> helixes[4];
  helixes[0] = MakeOneHelix(ymin, y0, y1, r0, spec.divs, spec.pitch, startIdx[0]);
  y0 = -spec.inner_width + slopeWidth;
  helixes[1] =
      MakeOneHelix(ymin, y0, y1, r1, spec.divs, spec.pitch, startIdx[1]);
  y0 = -slopeWidth;
  helixes[2] =
      MakeOneHelix(ymin, y0, y1, r1, spec.divs, spec.pitch, startIdx[2]);
  y0 = 0;
  helixes[3] =
      MakeOneHelix(ymin, y0, y1, r0, spec.divs, spec.pitch, startIdx[3]);
  for (unsigned i = 0; i < 3; i++) {
    FillRow(helixes[i], helixes[i + 1], startIdx[i], startIdx[i + 1], mesh, y1);
  }
  MergeCloseVertices(mesh);
  return 0;
}
