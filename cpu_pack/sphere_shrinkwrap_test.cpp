#include "PackShrinkWrap.h"
#include "PackingScene.h"
#include "TrigMesh.h"

#include <cmath>
#include <cstdio>
#include <iostream>
#include <map>
#include <vector>

// M2: measure the net growth of ComputeShrinkWrapDistField / MarchingCubes
// on a single sphere of known radius. If the closing operation truly has
// zero net growth, the output hull's radius should equal the input sphere's
// radius, not radius + shrinkRadius. See plan.txt, M2.
//
//   ./sphere_shrinkwrap_test

namespace {

struct EdgeKey {
  unsigned a, b;
  bool operator<(const EdgeKey &o) const {
    return a < o.a || (a == o.a && b < o.b);
  }
};

unsigned MidpointIndex(std::map<EdgeKey, unsigned> &lookup,
                       std::vector<Vec3f> &verts, unsigned i0, unsigned i1) {
  EdgeKey key{std::min(i0, i1), std::max(i0, i1)};
  auto it = lookup.find(key);
  if (it != lookup.end()) {
    return it->second;
  }
  Vec3f mid = (verts[i0] + verts[i1]) * 0.5f;
  mid.normalize();
  unsigned idx = unsigned(verts.size());
  verts.push_back(mid);
  lookup[key] = idx;
  return idx;
}

// unit icosphere (radius 1), centered at origin.
TrigMesh MakeUnitIcosphere(int subdivisions) {
  const float X = .525731112119133606f;
  const float Z = .850650808352039932f;
  std::vector<Vec3f> verts = {
      {-X, 0, Z}, {X, 0, Z},  {-X, 0, -Z}, {X, 0, -Z}, {0, Z, X},  {0, Z, -X},
      {0, -Z, X}, {0, -Z, -X}, {Z, X, 0},   {-Z, X, 0}, {Z, -X, 0}, {-Z, -X, 0}};
  for (auto &v : verts) {
    v.normalize();
  }
  std::vector<Vec3u> trigs = {
      {0, 4, 1},  {0, 9, 4},  {9, 5, 4},  {4, 5, 8},  {4, 8, 1},
      {8, 10, 1}, {8, 3, 10}, {5, 3, 8},  {5, 2, 3},  {2, 7, 3},
      {7, 10, 3}, {7, 6, 10}, {7, 11, 6}, {11, 0, 6}, {0, 1, 6},
      {6, 1, 10}, {9, 0, 11}, {9, 11, 2}, {9, 2, 5},  {7, 2, 11}};

  for (int s = 0; s < subdivisions; s++) {
    std::map<EdgeKey, unsigned> lookup;
    std::vector<Vec3u> nextTrigs;
    nextTrigs.reserve(trigs.size() * 4);
    for (const Vec3u &t : trigs) {
      unsigned m0 = MidpointIndex(lookup, verts, t[0], t[1]);
      unsigned m1 = MidpointIndex(lookup, verts, t[1], t[2]);
      unsigned m2 = MidpointIndex(lookup, verts, t[2], t[0]);
      nextTrigs.push_back({t[0], m0, m2});
      nextTrigs.push_back({t[1], m1, m0});
      nextTrigs.push_back({t[2], m2, m1});
      nextTrigs.push_back({m0, m1, m2});
    }
    trigs = nextTrigs;
  }

  TrigMesh m;
  m.v.resize(verts.size() * 3);
  for (size_t i = 0; i < verts.size(); i++) {
    m.v[3 * i] = verts[i][0];
    m.v[3 * i + 1] = verts[i][1];
    m.v[3 * i + 2] = verts[i][2];
  }
  m.t.resize(trigs.size() * 3);
  for (size_t i = 0; i < trigs.size(); i++) {
    m.t[3 * i] = trigs[i][0];
    m.t[3 * i + 1] = trigs[i][1];
    m.t[3 * i + 2] = trigs[i][2];
  }
  return m;
}

// mean/min/max distance of every hull vertex from the origin.
void RadiusStats(const TrigMesh &m, float &meanR, float &minR, float &maxR) {
  size_t n = m.GetNumVerts();
  meanR = 0.0f;
  minR = 1e10f;
  maxR = -1e10f;
  for (size_t i = 0; i < n; i++) {
    Vec3f p(m.v[3 * i], m.v[3 * i + 1], m.v[3 * i + 2]);
    float r = p.norm();
    meanR += r;
    minR = std::min(minR, r);
    maxR = std::max(maxR, r);
  }
  if (n > 0) {
    meanR /= float(n);
  }
}

void RunCase(float sphereRadius, float shrinkRadius, float voxelSize) {
  PackingScene scene;
  scene.outputFolder = "F:/meshes/fruit_hand/out_melone_test";

  MeshInfo item;
  item.mesh = MakeUnitIcosphere(4);
  item.mesh.scale(sphereRadius);
  item.name = "sphere";
  scene.items.push_back(item);

  RigidTransform tran;
  scene.instances.push_back(InstanceInfo(0, tran));

  TrigMesh hull = ComputeShrinkWrapMesh(scene, shrinkRadius, voxelSize);
  if (hull.GetNumTrigs() == 0) {
    std::cout << "sphereR=" << sphereRadius << " shrinkR=" << shrinkRadius
              << " voxelSize=" << voxelSize << " -> EMPTY HULL\n";
    return;
  }

  float meanR, minR, maxR;
  RadiusStats(hull, meanR, minR, maxR);
  std::printf(
      "sphereR=%.3f shrinkR=%.3f voxelSize=%.3f -> hull meanR=%.4f "
      "(minR=%.4f maxR=%.4f) delta=%.4f (=%.2f voxels)\n",
      sphereRadius, shrinkRadius, voxelSize, meanR, minR, maxR,
      meanR - sphereRadius, (meanR - sphereRadius) / voxelSize);
}

}  // namespace

int main() {
  std::cout.setf(std::ios::unitbuf);

  // sweep shrinkRadius and voxelSize independently to see how net growth
  // (hull meanR - sphereR) depends on each, on a shape with a known,
  // exact answer (a sphere has no holes to close, so ideally net growth
  // is 0 regardless of shrinkRadius).
  float sphereRadius = 2.0f;

  std::cout << "-- vary shrinkRadius at voxelSize=0.1 --\n";
  for (float shrinkRadius : {0.2f, 0.5f, 1.0f, 2.0f}) {
    RunCase(sphereRadius, shrinkRadius, 0.1f);
  }

  std::cout << "-- vary voxelSize at shrinkRadius=0.5 --\n";
  for (float voxelSize : {0.05f, 0.1f, 0.25f, 0.5f}) {
    RunCase(sphereRadius, 0.5f, voxelSize);
  }

  return 0;
}
