#include "ElementMeshUtil.h"

#include <unordered_set>
#include <unordered_map>

void ComputeSurfaceMesh(const ElementMesh& em, TrigMesh& m,
                        std::vector<uint32_t>& meshToEMVert) {}

struct hash_edge {
  size_t operator()(const Edge& e) const { return e.v[0] + 50951 * e.v[1]; }
};

void ComputeWireframeMesh(const ElementMesh& em, TrigMesh& m,
                          std::vector<Edge>& edges) {
  std::unordered_set<Edge, hash_edge> edgeSet;
  for (size_t i = 0; i < em.e.size(); i++) {
    const Element* ele = em.e[i].get();
    unsigned numEdges = ele->NumEdges();
    for (size_t ei = 0; ei < numEdges; ei++) {
      unsigned v1 = 0, v2 = 0;
      ele->Edge(ei, v1, v2);
      Edge edge(v1, v2);
      edgeSet.insert(edge);
    }
  }
  edges = std::vector<Edge>(edgeSet.begin(), edgeSet.end());
  
  unsigned numVerts = 6 * edges.size();
  unsigned numTrigs = 6 * edges.size();
  m.t.resize(3 * numTrigs);
  m.v.resize(3 * numVerts, 0);

  const unsigned PRISM_TRIGS[6][3] = {{1, 0, 3}, {1, 3, 4}, {1, 4, 5},
                                      {1, 5, 2}, {0, 2, 3}, {3, 2, 5}};
  for (unsigned i = 0; i < edges.size(); i++) {
    unsigned t0Idx = 6 * i;
    unsigned v0Idx = 6 * i;
    for (unsigned ti = 0; ti < 6; ti++) {
      unsigned tIdx = t0Idx + ti;
      m.t[3 * tIdx] = v0Idx + PRISM_TRIGS[ti][0];
      m.t[3 * tIdx + 1] = v0Idx + PRISM_TRIGS[ti][1];
      m.t[3 * tIdx + 2] = v0Idx + PRISM_TRIGS[ti][2];
    }
  }
  UpdateWireframePosition(em, m, edges);
}

void UpdateWireframePosition(const ElementMesh& em, TrigMesh& m,
                             std::vector<Edge>& edges) {
  const float PRISM_VERTS[3][2] = {
      {0.5, -0.2887}, {-0.5, -0.2887}, {0, 0.5774}};
  float beamThickness = 0.5f;
  const float EPS = 1e-6;
  for (unsigned i = 0; i < edges.size(); i++) {
    Vec3f v1 = em.x[edges[i].v[0]];
    Vec3f v2 = em.x[edges[i].v[1]];
    Vec3f eVec = v2 - v1;
    float len = eVec.norm();
    // 4   5
    //   3
    //
    // 1   2
    //   0
    unsigned v0Idx = 6 * i;

    if (len < EPS) {
      continue;
    }
    eVec = (1.0f / len) * eVec;
    Vec3f yAxis(0, 1, 0);

    if (eVec[1] > 0.95) {
      yAxis = Vec3f(0, 0, -1);
    } else if (eVec[1] < -0.95) {
      yAxis = Vec3f(0, 0, 1);
    }
    Vec3f xAxis = yAxis.cross(eVec);
    xAxis.normalize();
    yAxis = eVec.cross(xAxis);

    for (unsigned vi = 0; vi < 3; vi++) {
      Vec3f pos = v1 + beamThickness * (eVec + xAxis * PRISM_VERTS[vi][0] +
                                        yAxis * PRISM_VERTS[vi][1]);
      unsigned vIdx = v0Idx + vi;
      *(Vec3f*)(&m.v[3 * vIdx]) = pos;

      Vec3f pos1 = pos + (len - 2 * beamThickness) * eVec;
      *(Vec3f*)(&m.v[3 * (vIdx + 3)]) = pos1;
    }
  }
}
