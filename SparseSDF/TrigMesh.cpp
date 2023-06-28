#include "TrigMesh.h"

#include "stl_reader.h"
//#include "meshreader/stepFileReader_HOOPS.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>

#include "ObjLoader.h"
#include "Vec3.h"

int TrigMesh::LoadStl(const std::string& meshFile) {
  std::vector<unsigned> solids;
  std::vector<float> n;
  bool success = false;
  try {
    success = stl_reader::ReadStlFile(meshFile.c_str(), v, n, t, solids);
  } catch (...) {
  }
  if (!success) {
    std::cout << "error reading stl file " << meshFile << "\n";
    return -1;
  }
  ComputeTrigNormals();
  // ComputePseudoNormals();
  return 0;
}

int TrigMesh::LoadObj(const std::string& meshFile) {
  int status = LoadOBJFile(meshFile, t, v, uv);
  if (status < 0) {
    return status;
  }
  ComputeTrigNormals();
  return 0;
}

Vec3f TrigMesh::GetNormal(unsigned tIdx, const Vec3f& bary) {
  Vec3f n;
  float eps = 1e-5;
  unsigned zeroCount = 0;
  unsigned zeroIdx = 0, nonZeroIdx = 0;
  for (unsigned i = 0; i < 3; i++) {
    if (bary[i] < eps) {
      zeroCount++;
      zeroIdx = i;
    } else {
      nonZeroIdx = i;
    }
  }

  if (zeroCount == 1) {
    // edge
    size_t ei = (zeroIdx + 1) % 3;
    size_t i = 3 * size_t(tIdx) + ei;
    size_t eIdx = te[i];
    n = ne[eIdx];
  } else if (zeroCount == 2) {
    // vertex
    size_t vIdx = t[3 * size_t(tIdx) + nonZeroIdx];
    n = nv[vIdx];
  } else {
    // face
    size_t i0 = 3 * size_t(tIdx);
    n[0] = nt[i0];
    n[1] = nt[i0 + 1];
    n[2] = nt[i0 + 2];
  }
  return n;
}

void TrigMesh::ComputeTrigEdges() {
  std::map<Edge, unsigned> edgeMap;
  te.resize(t.size());
  for (size_t tIdx = 0; tIdx < t.size(); tIdx += 3) {
    for (unsigned i = 0; i < 3; i++) {
      unsigned j = (i + 1) % 3;
      Edge e(t[tIdx + i], t[tIdx + j]);
      auto it = edgeMap.find(e);
      unsigned edgeIdx;
      if (it != edgeMap.end()) {
        edgeIdx = it->second;
      } else {
        edgeIdx = edgeMap.size();
        edgeMap.insert({e, edgeIdx});
      }
      te[tIdx + i] = edgeIdx;
    }
  }
  // initialize edge normals to 0
  ne.resize(edgeMap.size(), Vec3f(0.0f));
}

void TrigMesh::ComputePseudoNormals() {
  ComputeTrigEdges();
  // std::vector<uint8_t> edgeCount(ne.size());
  for (size_t tIdx = 0; tIdx < t.size(); tIdx += 3) {
    for (unsigned i = 0; i < 3; i++) {
      unsigned edgeIdx = te[tIdx + i];
      // edgeCount[edgeIdx] ++;
      ne[edgeIdx] += Vec3f(nt[tIdx], nt[tIdx + 1], nt[tIdx + 2]);
    }
  }

  for (size_t eIdx = 0; eIdx < ne.size(); eIdx++) {
    // ne[eIdx] /= edgeCount[eIdx];
    ne[eIdx].normalize();
  }
  ComputeVertNormals();
}

void TrigMesh::ComputeVertNormals() {
  size_t numVerts = v.size() / 3;
  // std::vector<float> weight(numVerts, 0.0f);
  nv.resize(numVerts, Vec3f(0.0f));
  const float eps = 1e-10;
  for (size_t tIdx = 0; tIdx < t.size(); tIdx += 3) {
    Vec3f vert[3];
    for (unsigned vi = 0; vi < 3; vi++) {
      size_t vIdx = t[tIdx + vi];
      vert[vi] = Vec3f(v[3 * vIdx], v[3 * vIdx + 1], v[3 * vIdx + 2]);
    }
    Vec3f n(nt[tIdx], nt[tIdx + 1], nt[tIdx + 2]);
    for (unsigned v0 = 0; v0 < 3; v0++) {
      unsigned v1 = (v0 + 1) % 3;
      unsigned v2 = (v0 + 2) % 3;
      Vec3f e1 = vert[v1] - vert[v0];
      Vec3f e2 = vert[v2] - vert[v0];
      float sqrLen1 = e1.norm2();
      float sqrLen2 = e2.norm2();
      float denom = sqrLen1 * sqrLen2;
      float angle = 0.0f;
      if (denom > eps) {
        float dot = e1.dot(e2);
        angle = std::acos(dot / std::sqrt(denom));
      }
      size_t vIdx = t[tIdx + v0];
      //  weight[vIdx] += angle;
      nv[vIdx] += angle * n;
    }
  }

  for (size_t vIdx = 0; vIdx < nv.size(); vIdx++) {
    // if (weight[vIdx] > eps) {
    //   nv[vIdx] /= weight[vIdx];
    nv[vIdx].normalize();
    //}
  }
}

// compute triangle area.
float TrigMesh::GetArea(unsigned tIdx) const {
  unsigned vIdx[3] = {t[tIdx * 3], t[tIdx * 3 + 1], t[tIdx * 3 + 2]};
  Vec3f v0(v[vIdx[0] * 3], v[vIdx[0] * 3 + 1], v[vIdx[0] * 3 + 2]);
  Vec3f e10 =
      Vec3f(v[vIdx[1] * 3], v[vIdx[1] * 3 + 1], v[vIdx[1] * 3 + 2]) - v0;
  Vec3f e20 =
      Vec3f(v[vIdx[2] * 3], v[vIdx[2] * 3 + 1], v[vIdx[2] * 3 + 2]) - v0;

  return (e10.cross(e20)).norm() * 0.5f;
}

// https://math.stackexchange.com/questions/18686/uniform-random-point-in-triangle-in-3d
//  v0, v1, v2, three vertices. r1, r2 \in U[0,1]
Vec3f TrigMesh::GetRandomSample(const unsigned tIdx, const float r1,
                                const float r2) const {
  unsigned vIdx[3] = {t[tIdx * 3], t[tIdx * 3 + 1], t[tIdx * 3 + 2]};
  Vec3f v0(v[vIdx[0] * 3], v[vIdx[0] * 3 + 1], v[vIdx[0] * 3 + 2]);
  Vec3f v1(v[vIdx[1] * 3], v[vIdx[1] * 3 + 1], v[vIdx[1] * 3 + 2]);
  Vec3f v2(v[vIdx[2] * 3], v[vIdx[2] * 3 + 1], v[vIdx[2] * 3 + 2]);
  return (1.f - sqrtf(r1)) * v0 + (sqrtf(r1) * (1.f - r2)) * v1 +
         (r2 * sqrtf(r1)) * v2;
}

// also returns trig normal and a tangent vector
void TrigMesh::GetRandomSample(const unsigned tIdx, const float r1,
                               const float r2, Vec3f& pos, Vec3f& normal,
                               Vec3f& tangent) const {
  unsigned vIdx[3] = {t[tIdx * 3], t[tIdx * 3 + 1], t[tIdx * 3 + 2]};
  Vec3f trigV[3] = {
      Vec3f(v[vIdx[0] * 3], v[vIdx[0] * 3 + 1], v[vIdx[0] * 3 + 2]),
      Vec3f(v[vIdx[1] * 3], v[vIdx[1] * 3 + 1], v[vIdx[1] * 3 + 2]),
      Vec3f(v[vIdx[2] * 3], v[vIdx[2] * 3 + 1], v[vIdx[2] * 3 + 2])};

  pos = (1.f - sqrtf(r1)) * trigV[0] + (sqrtf(r1) * (1.f - r2)) * trigV[1] +
        (r2 * sqrtf(r1)) * trigV[2];
  normal = Vec3f(nt[tIdx * 3], nt[tIdx * 3 + 1], nt[tIdx * 3 + 2]);
  float dist = -1.0;
  int maxDistIdx = 0;
  for (int i = 0; i < 3; i++) {
    float curDist = (pos - trigV[i]).norm2();
    if (curDist > dist) {
      dist = curDist;
      maxDistIdx = i;
    }
  }
  tangent = pos - trigV[maxDistIdx];
  tangent.normalize();
}

void TrigMesh::ComputeTrigNormals() {
  nt.resize(t.size());
  for (size_t i = 0; i < t.size(); i += 3) {
    Vec3f vert[3];
    for (unsigned j = 0; j < 3; j++) {
      size_t vidx = 3 * size_t(t[i + j]);
      vert[j][0] = v[vidx];
      vert[j][1] = v[vidx + 1];
      vert[j][2] = v[vidx + 2];
    }
    vert[1] -= vert[0];
    vert[2] -= vert[0];
    Vec3f n = vert[1].cross(vert[2]);
    n.normalize();
    nt[i] = n[0];
    nt[i + 1] = n[1];
    nt[i + 2] = n[2];
  }
}

unsigned TrigMesh::GetIndex(size_t tIdx, unsigned jIdx) const {
  return t[3 * tIdx + jIdx];
}

Vec3f TrigMesh::GetVertex(size_t vIdx) const {
  return Vec3f(v[3 * vIdx], v[3 * vIdx + 1], v[3 * vIdx + 2]);
}

Vec3f TrigMesh::GetTriangleVertex(size_t tIdx, unsigned jIdx) const {
  return GetVertex(GetIndex(tIdx, jIdx));
}

Triangle TrigMesh::GetTriangleVerts(size_t tIdx) const {
  Triangle trig;
  trig.v[0] = *(Vec3f*)(&v[3 * t[3 * tIdx]]);
  trig.v[1] = *(Vec3f*)(&v[3 * t[3 * tIdx + 1]]);
  trig.v[2] = *(Vec3f*)(&v[3 * t[3 * tIdx + 2]]);
  return trig;
}

Vec3f TrigMesh::GetTrigNormal(size_t tIdx) const {
  return Vec3f(nt[3 * tIdx], nt[3 * tIdx + 1], nt[3 * tIdx + 2]);
}

void TrigMesh::scale(float s) {
  for (size_t i = 0; i < v.size(); i++) {
    for (unsigned d = 0; d < 3; d++) {
      v[3 * i + d] *= s;
    }
  }
}

void TrigMesh::translate(float dx, float dy, float dz) {
  for (size_t i = 0; i < v.size(); i++) {
    v[3 * i] += dx;
    v[3 * i + 1] += dy;
    v[3 * i + 2] += dz;
  }
}

void TrigMesh::append(const TrigMesh& m) {
  size_t offset = v.size() / 3;
  size_t ot = t.size();
  v.insert(v.end(), m.v.begin(), m.v.end());
  t.insert(t.end(), m.t.begin(), m.t.end());
  for (size_t ii = ot; ii < t.size(); ii++) {
    t[ii] += (int)offset;
  }
}

void TrigMesh::SaveObj(const std::string& filename) {
  std::ofstream out(filename);
  if (!out.is_open()) return;

  size_t numVerts = v.size() / 3;
  bool hasVertNormal = (nv.size() == v.size() / 3);
  for (size_t vIdx = 0; vIdx < numVerts; vIdx++) {
    out << "v " << v[3 * vIdx] << " " << v[3 * vIdx + 1] << " "
        << v[3 * vIdx + 2] << "\n";
    if (hasVertNormal) {
      out << "vn " << nv[vIdx][0] << " " << nv[vIdx][1] << " " << nv[vIdx][2]
          << "\n";
    }
  }

  bool hasUv = (uv.size() == t.size());
  if (hasUv) {
    for (size_t vIdx = 0; vIdx < uv.size(); vIdx++) {
      out << "vt " << uv[vIdx][0] << " " << uv[vIdx][1] << "\n";
    }
  }
  size_t numTrig = t.size() / 3;
  for (size_t tIdx = 0; tIdx < numTrig; tIdx++) {
    out << "f";
    for (unsigned vi = 0; vi < 3; vi++) {
      size_t vIdx = t[3 * tIdx + vi] + 1;
      out << " " << vIdx;
      if (hasUv) {
        out << "/" << (1+3*tIdx+vi);
      }
      if (hasVertNormal) {
        if (!hasUv) {
          out << "/";
        }
        out << "/" << vIdx;
      }
    }
    out << "\n";
  }
  out.close();
}

void TrigMesh::SaveObj(const std::string& filename,
                       const std::vector<Vec3f>& pts) {
  std::ofstream out(filename);
  if (!out.is_open()) return;

  for (const auto& p : pts) {
    out << "v " << std::setprecision(12) << p[0] << " " << p[1] << " " << p[2]
        << "\n";
  }
  out.close();
}

TrigMesh SubSet(const TrigMesh& m, const std::vector<size_t>& trigs) {
  TrigMesh out;
  for (size_t i = 0; i < trigs.size(); i++) {
    unsigned tIdx = trigs[i];
    for (unsigned j = 0; j < 3; j++) {
      for (unsigned d = 0; d < 3; d++) {
        unsigned localv = 3 * tIdx + j;
        unsigned vIdx = m.t[localv];
        out.v.push_back(m.v[3 * vIdx + d]);
      }
      out.t.push_back(3 * i + j);
    }
  }
  return out;
}

TrigMesh MakeCube(const Vec3f& mn, const Vec3f& mx) {
  ///@brief cube [0,1]^3
  float CUBE_VERT[8][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                           {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};

  unsigned CUBE_TRIG[12][3] = {
      {0, 3, 1}, {1, 3, 2},  // z=0face
      {7, 4, 5}, {7, 5, 6},  // z=1 face
      {5, 4, 0}, {5, 0, 1},  // y=0 face
      {3, 6, 2}, {3, 7, 6},  // y=1 face
      {4, 3, 0}, {4, 7, 3},  // x=0 face
      {6, 5, 1}, {1, 2, 6},  // x=1 face
  };
  TrigMesh m;
  for (size_t v = 0; v < 8; v++) {
    for (size_t d = 0; d < 3; d++) {
      float f = CUBE_VERT[v][d] * (mx[d] - mn[d]) + mn[d];
      m.v.push_back(f);
    }
  }
  for (size_t t = 0; t < 12; t++) {
    for (size_t d = 0; d < 3; d++) {
      m.t.push_back(CUBE_TRIG[t][d]);
    }
  }
  return m;
}
