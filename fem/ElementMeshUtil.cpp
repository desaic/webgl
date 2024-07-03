#include "ElementMeshUtil.h"
#include "BBox.h"
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include "Array3D.h"
#include <algorithm>

static Vec3u linearToGrid(unsigned l, const Vec3u& size) {
  Vec3u idx;
  idx[2] = l / (size[0] * size[1]);
  unsigned r = l - idx[2] * size[0] * size[1];
  idx[1] = r / size[0];
  idx[0] = l % (size[0]);
  return idx;
}

static unsigned linearIdx(const Vec3u& idx, const Vec3u& size) {
  unsigned l;
  l = idx[0] + idx[1] * size[0] + idx[2] * size[0] * size[1];
  return l;
}

void ComputeSurfaceMesh(const ElementMesh& em, TrigMesh& m,
                        std::vector<uint32_t>& meshToEMVert,
                        float drawingScale) {
  for (size_t i = 0; i < em.e.size(); i++) {
    const Element* ele = em.e[i].get();
  }
}

struct hash_edge {
  size_t operator()(const Edge& e) const { return e.v[0] + 50951 * e.v[1]; }
};

void ComputeWireframeMesh(const ElementMesh& em, TrigMesh& m,
                          std::vector<Edge>& edges,
                          float drawingScale,
  float beamThickness) {
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
  UpdateWireframePosition(em, m, edges, drawingScale,
                          beamThickness);
}

void UpdateWireframePosition(const ElementMesh& em, TrigMesh& m,
                             std::vector<Edge>& edges, float drawingScale,
                             float beamThickness) {
  const float PRISM_VERTS[3][2] = {
      {0.5, -0.2887}, {-0.5, -0.2887}, {0, 0.5774}};
  const float EPS = 1e-6;
  for (unsigned i = 0; i < edges.size(); i++) {
    Vec3f v1 = drawingScale * em.x[edges[i].v[0]];
    Vec3f v2 = drawingScale * em.x[edges[i].v[1]];
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
      Vec3f pos = v1 + beamThickness * (0.2f*eVec + xAxis * PRISM_VERTS[vi][0] +
                                        yAxis * PRISM_VERTS[vi][1]);
      unsigned vIdx = v0Idx + vi;
      *(Vec3f*)(&m.v[3 * vIdx]) = pos;

      Vec3f pos1 = pos + (len - 0.2f * beamThickness) * eVec;
      *(Vec3f*)(&m.v[3 * (vIdx + 3)]) = pos1;
    }
  }
}

void PullRight(const Vec3f& force, float dist, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (box.vmax[0] - em.X[i][0] < dist) {
      em.fe[i] += force;
    }
  }
}

void PullLeft(const Vec3f& force, float dist, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (em.X[i][0] - box.vmin[0] < dist) {
      em.fe[i] += force;
    }
  }
}

void AddGravity(const Vec3f& G, ElementMesh& em) {
  for (size_t i = 0; i < em.X.size(); i++) {
    em.fe[i] += G;
  }
}

void FixLeft(float dist, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (em.X[i][0] - box.vmin[0] < dist) {
      em.fixedDOF[3 * i] = true;
      em.fixedDOF[3 * i + 1] = true;
      em.fixedDOF[3 * i + 2] = true;
    }
  }
}

void FixRight(float dist, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (box.vmax[0] - em.X[i][0] < dist) {
      em.fixedDOF[3 * i] = true;
      em.fixedDOF[3 * i + 1] = true;
      em.fixedDOF[3 * i + 2] = true;
    }
  }
}

// for x > xmax-range, x-=distance
void MoveRightEnd(float range, float distance, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (box.vmax[0] - em.X[i][0] < range) {
      em.x[i][0] += distance;
    }
  }
}

void FixMiddle(float ratio, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  float dist = ratio * (box.vmax[0] - box.vmin[0]);
  float midx = 0.5f * (box.vmax[0] + box.vmin[0]);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (std::abs(em.X[i][0] - midx) < dist) {
      em.fixedDOF[3 * i] = true;
      em.fixedDOF[3 * i + 1] = true;
      em.fixedDOF[3 * i + 2] = true;
    }
  }
}

void PullMiddle(const Vec3f& force, float ratio, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  float dist = ratio * (box.vmax[0] - box.vmin[0]);
  float midx = 0.5f * (box.vmax[0] + box.vmin[0]);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (std::abs(em.X[i][0] - midx) < dist) {
      em.fe[i] += force;
    }
  }
}
void FixFloorLeft(float ratio, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  float dist = ratio * (box.vmax[0] - box.vmin[0]);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (em.X[i][0] - box.vmin[0] < dist) {
      em.fixedDOF[3 * i + 1] = true;
    }
  }
}

void SetFloorForRightSide(float y1, float ratio, ElementMesh& em) {
  BBox box;
  ComputeBBox(em.X, box);
  float dist = ratio * (box.vmax[0] - box.vmin[0]);
  for (size_t i = 0; i < em.X.size(); i++) {
    if (box.vmax[0] - em.X[i][0] < dist) {
      em.lb[i][1] = y1;
    }
  }
}


int LoadX(ElementMesh& em, const std::string& inFile) {
  std::ifstream in(inFile);
  size_t xSize = 0;
  in >> xSize;
  if (xSize != em.X.size()) {
    std::cout << "mismatch x size vs mesh size.\n";
    return -1;
  }
  em.x.resize(xSize);
  for (size_t i = 0; i < em.x.size(); i++) {
    in >> em.x[i][0] >> em.x[i][1] >> em.x[i][2];
  }
  return 0;
}

Vec3f Trilinear(const Element& hex, const std::vector<Vec3f>& X,
                const std::vector<Vec3f>& x, Vec3f& v) {
  Vec3f out(0, 0, 0);
  Vec3f X0 = X[hex[0]];
  float h = X[hex[1]][2] - X0[2];
  float w[3];
  for (unsigned i = 0; i < 3; i++) {
    w[i] = (v[i] - X0[i]) / h;
    w[i] = std::clamp(w[i], 0.0f, 1.0f);
  }
  Vec3f z00 =
      (1 - w[2]) * (x[hex[0]] - X[hex[0]]) + w[2] * (x[hex[1]] - X[hex[1]]);
  Vec3f z01 =
      (1 - w[2]) * (x[hex[2]] - X[hex[2]]) + w[2] * (x[hex[3]] - X[hex[3]]);
  Vec3f z10 =
      (1 - w[2]) * (x[hex[4]] - X[hex[4]]) + w[2] * (x[hex[5]] - X[hex[5]]);
  Vec3f z11 =
      (1 - w[2]) * (x[hex[6]] - X[hex[6]]) + w[2] * (x[hex[7]] - X[hex[7]]);
  Vec3f y0 = (1 - w[1]) * z00 + w[1] * z01;
  Vec3f y1 = (1 - w[1]) * z10 + w[1] * z11;
  Vec3f u = (1 - w[0]) * y0 + w[0] * y1;
  out = v + u;
  return out;
}

void EmbedMesh() {
  ElementMesh em;
  em.LoadTxt("F:/dump/vox6080.txt");
  LoadX(em, "F:/dump/x_6080_11.txt");
  TrigMesh mesh;
  mesh.LoadObj("F:/dolphin/meshes/gasket/gasket6080_in.obj");
  const float meterTomm = 1000.0f;
  for (auto& X : em.X) {
    X = meterTomm * X;
  }
  for (auto& x : em.x) {
    x = meterTomm * x;
  }
  Vec3u gridSize;
  float h = 2.7f;
  Array3D<unsigned> grid;
  BBox box;
  ComputeBBox(em.X, box);
  for (unsigned i = 0; i < 3; i++) {
    gridSize[i] = unsigned((box.vmax[i] - box.vmin[i]) / h - 0.5f) + 1;
  }
  grid.Allocate(gridSize[0], gridSize[1], gridSize[2], ~0);
  bool hasMax[3] = {0, 0, 0};
  for (size_t i = 0; i < em.e.size(); i++) {
    Vec3f center = em.X[(*em.e[i])[0]] + 0.5f * Vec3f(h, h, h);
    unsigned ix = unsigned(center[0] / h);
    unsigned iy = unsigned(center[1] / h);
    unsigned iz = unsigned(center[2] / h);
    grid(ix, iy, iz) = i;
    if (ix == gridSize[0] - 1) {
      hasMax[0] = true;
    }
    if (iy == gridSize[1] - 1) {
      hasMax[1] = true;
    }
    if (iz == gridSize[2] - 1) {
      hasMax[2] = true;
    }
  }
  // for each surface mesh vertex, assign it to an element.
  std::vector<unsigned> vertEle(mesh.GetNumVerts(), 0);
  std::vector<float> newVerts(mesh.v.size(), 0);
  Vec3f delta[6] = {{0, 0, -0.1f}, {0, 0, 0.1f},  {0, -0.1f, 0},
                    {0, 0.1f, 0},  {-0.1f, 0, 0}, {0.1f, 0, 0}};
  for (size_t i = 0; i < mesh.GetNumVerts(); i++) {
    // find closest cuboid center
    Vec3u gridIdx;
    Vec3f v0 = *(Vec3f*)(&mesh.v[3 * i]);
    int intIndex[3];
    for (unsigned j = 0; j < 3; j++) {
      intIndex[j] = int(v0[j] / h);
      intIndex[j] = std::clamp(intIndex[j], 0, int(gridSize[j]) - 1);
      gridIdx[j] = unsigned(intIndex[j]);
    }
    std::vector<Vec3u> candidates(7);
    unsigned eidx = em.e.size();
    candidates[0] = gridIdx;
    for (unsigned c = 0; c < 6; c++) {
      Vec3f v1 = v0 + delta[c];
      for (unsigned j = 0; j < 3; j++) {
        intIndex[j] = int(v1[j] / h);
        intIndex[j] = std::clamp(intIndex[j], 0, int(gridSize[j]) - 1);
        gridIdx[j] = unsigned(intIndex[j]);
      }
      candidates[c + 1] = gridIdx;
    }
    for (size_t c = 0; c < candidates.size(); c++) {
      gridIdx = candidates[c];
      unsigned ei = grid(gridIdx[0], gridIdx[1], gridIdx[2]);
      if (ei < em.e.size()) {
        eidx = ei;
        break;
      }
    }
    if (eidx >= em.e.size()) {
      std::cout << i << " ";
      std::cout << v0[0] << " " << v0[1] << " " << v0[2] << "\n";
    }
    Vec3f moved = Trilinear(*em.e[eidx], em.X, em.x, v0);
    newVerts[3 * i] = moved[0];
    newVerts[3 * i + 1] = moved[1];
    newVerts[3 * i + 2] = moved[2];
  }
  mesh.v = newVerts;
  mesh.SaveObj("F:/dump/deformed.obj");
}

void SaveHexTxt(const std::string& file, const std::vector<Vec3f>& X,
                const std::vector<std::array<unsigned, 8> > elts) {
  if (elts.size() == 0) {
    return;
  }
  const unsigned NUM_HEX_VERTS = elts[0].size();
  std::ofstream out(file);
  out << "verts " << X.size() << "\n";
  out << "elts " << elts.size() << "\n";
  for (auto v : X) {
    out << v[0] << " " << v[1] << " " << v[2] << "\n";
  }
  for (size_t i = 0; i < elts.size(); i++) {
    out << NUM_HEX_VERTS << " ";
    for (size_t j = 0; j < NUM_HEX_VERTS; j++) {
      out << elts[i][j] << " ";
    }
    out << "\n";
  }
}

void VoxGridToHexMesh(const Array3D8u& grid,
   const Vec3f & dx, const Vec3f & origin, std::vector<Vec3f>& X,
                      std::vector<std::array<unsigned, 8> >& elts) {
  std::map<unsigned, unsigned> vertMap;
  Vec3u voxSize = grid.GetSize();
  Vec3u vertSize = voxSize;
  vertSize += Vec3u(1, 1, 1);
  const std::vector<uint8_t>& v = grid.GetData();
  const size_t NUM_HEX_VERTS = 8;
  unsigned HEX_VERTS[8][3] = {{0, 0, 0}, {0, 0, 1}, {0, 1, 0}, {0, 1, 1},
                              {1, 0, 0}, {1, 0, 1}, {1, 1, 0}, {1, 1, 1}};
  std::vector<unsigned> verts;
  for (size_t i = 0; i < v.size(); i++) {
    if (!v[i]) {
      continue;
    }
    Vec3u voxIdx = linearToGrid(i, voxSize);
    std::array<unsigned, NUM_HEX_VERTS> ele;
    for (size_t j = 0; j < NUM_HEX_VERTS; j++) {
      Vec3u vi = voxIdx;
      vi[0] += HEX_VERTS[j][0];
      vi[1] += HEX_VERTS[j][1];
      vi[2] += HEX_VERTS[j][2];
      unsigned vl = linearIdx(vi, vertSize);
      auto it = vertMap.find(vl);
      unsigned vIdx;
      if (it == vertMap.end()) {
        vIdx = vertMap.size();
        vertMap[vl] = vIdx;
        verts.push_back(vl);
      } else {
        vIdx = vertMap[vl];
      }
      ele[j] = vIdx;
    }
    elts.push_back(ele);
  }
  for (auto v : verts) {
    unsigned l = v;
    Vec3u vertIdx = linearToGrid(l, vertSize);
    Vec3f coord;
    coord[0] = float(vertIdx[0]) * dx[0];
    coord[1] = float(vertIdx[1]) * dx[1];
    coord[2] = float(vertIdx[2]) * dx[2];
    coord += origin;
    X.push_back(coord);
  }
}

ElementMesh MakeHexMeshBlock(Vec3u size, const Vec3f& dx) {
  ElementMesh mesh;
  Array3D8u grid(size[0], size[1], size[2]);
  grid.Fill(1);
  std::vector<std::array<unsigned, 8> > elts;
  VoxGridToHexMesh(grid, dx, Vec3f(0), mesh.X, elts);
  mesh.InitElements();
  mesh.x = mesh.X;
  for (size_t i = 0; i < elts.size(); i++) {
    mesh.e.push_back(std::make_unique<HexElement>());
    for (unsigned j = 0; j < mesh.e.back()->NumVerts(); j++) {
      (*mesh.e.back())[j] = elts[i][j];
    }
  }
  return mesh;
}
