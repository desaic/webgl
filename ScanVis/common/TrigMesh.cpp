#include "TrigMesh.h"
#include "thirdparty/stl_reader/stl_reader.h"
#include "Vec3.h"
#include <iostream>
#include <fstream>
#include <map>

int TrigMesh::LoadStl(const std::string& meshFile) {
  std::vector<unsigned > solids;
  std::vector<float> n;
  bool success = false;
  try {
    success = stl_reader::ReadStlFile(meshFile.c_str(), v, n, t, solids);
  }
  catch (...) {
  }
  if (!success) {
    std::cout << "error reading stl file " << meshFile << "\n";
    return -1;
  }
  ComputeTrigNormals();
  //ComputePseudoNormals();
  return 0;
}

Vec3f TrigMesh::GetNormal(unsigned tIdx, const Vec3f& bary)
{
  Vec3f n;
  float eps = 1e-5;
  unsigned zeroCount = 0;
  unsigned zeroIdx=0, nonZeroIdx=0;
  for (unsigned i = 0; i < 3; i++) {
    if (bary[i] < eps) {
      zeroCount++;
      zeroIdx = i;
    }
    else {
      nonZeroIdx = i;
    }
  }
  
  if (zeroCount == 1) {
    //edge
    size_t ei = (zeroIdx + 1) % 3;
    size_t i = 3* size_t(tIdx) + ei;
    size_t eIdx = te[i];
    n = ne[eIdx];
  }
  else if(zeroCount == 2){
    //vertex
    size_t vIdx = t[3 * size_t(tIdx) + nonZeroIdx];
    n = nv[vIdx];
  }
  else {
    //face
    size_t i0 = 3 * size_t(tIdx);
    n[0] = nt[i0];
    n[1] = nt[i0+1];
    n[2] = nt[i0+2];
  }
  return n;
}

struct Edge {
  unsigned v[2];
  //sorts the indice increasing order
  Edge(unsigned v0, unsigned v1) {
    if (v0 <= v1) {
      v[0] = v0;
      v[1] = v1;
    }
    else {
      v[0] = v1;
      v[1] = v0;
    }
  }
  bool operator<(const Edge& o) const
  {
    if (v[0] < o.v[0]) {
      return true;
    }    
    return (v[0] == o.v[0] && v[1] < o.v[1]);
  }
};

void TrigMesh::ComputeTrigEdges()
{
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
      }
      else {
        edgeIdx = edgeMap.size();
        edgeMap.insert({ e,edgeIdx });
      }
      te[tIdx + i] = edgeIdx;
    }
  }
  //initialize edge normals to 0
  ne.resize(edgeMap.size(), Vec3f(0.0f));
}

void TrigMesh::ComputePseudoNormals()
{
  ComputeTrigEdges();
  //std::vector<uint8_t> edgeCount(ne.size());
  for (size_t tIdx = 0; tIdx < t.size(); tIdx += 3) {
    for (unsigned i = 0; i < 3; i++) {
      unsigned edgeIdx = te[tIdx + i];
      //edgeCount[edgeIdx] ++;
      ne[edgeIdx] += Vec3f(nt[tIdx], nt[tIdx + 1], nt[tIdx + 2]);
    }
  }

  for (size_t eIdx = 0; eIdx < ne.size(); eIdx++) {
    //ne[eIdx] /= edgeCount[eIdx];
    ne[eIdx].normalize();
  }
  ComputeVertNormals();
}

void TrigMesh::ComputeVertNormals()
{
  size_t numVerts = v.size() / 3;
  //std::vector<float> weight(numVerts, 0.0f);
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

  for (size_t vIdx = 0; vIdx < nv.size(); vIdx ++) {
    //if (weight[vIdx] > eps) {
    //  nv[vIdx] /= weight[vIdx];
      nv[vIdx].normalize();
    //}
  }
}

void TrigMesh::ComputeTrigNormals()
{
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
    nt[i+1] = n[1];
    nt[i+2] = n[2];
  }
}

void TrigMesh::scale(float s)
{
  for (size_t i = 0; i < v.size(); i++) {
    for (unsigned d = 0; d < 3; d++) {
      v[3*i + d] *= s;
    }
  }
}

void TrigMesh::translate(float dx, float dy, float dz)
{
  for (size_t i = 0; i < v.size(); i++) {
    v[3 * i] += dx;
    v[3 * i + 1] += dy;
    v[3 * i + 2] += dz;
  }
}

void TrigMesh::append(const TrigMesh& m)
{
  size_t offset = v.size();
  size_t ot = t.size();
  v.insert(v.end(), m.v.begin(), m.v.end());
  t.insert(t.end(), m.t.begin(), m.t.end());
  for (size_t ii = ot; ii < t.size(); ii++) {
    for (int jj = 0; jj < 3; jj++) {
      t[3*ii + jj] += (int)offset;
    }
  }
}

void TrigMesh::SaveObj(const std::string& filename)
{
  std::ofstream out(filename);
  size_t numVerts = v.size() / 3;
  bool hasVertNormal = (nv.size() == v.size()/3);
  for (size_t vIdx = 0; vIdx < numVerts; vIdx++) {
    out << "v " << v[3 * vIdx] << " " << v[3 * vIdx + 1] 
      << " " << v[3 * vIdx + 2] << "\n";
    if (hasVertNormal) {
      out << "vn " << nv[vIdx][0] << " " << nv[vIdx][1]
        << " " << nv[vIdx][2] << "\n";
    }
  }

  size_t numTrig = t.size() / 3;
  for (size_t tIdx = 0; tIdx < numTrig; tIdx++) {
    out << "f";
    for (unsigned vi = 0; vi < 3; vi++) {
      size_t vIdx = t[3 * tIdx + vi] + 1;
      out << " " << vIdx;
      if (hasVertNormal) {
        out << "//" << vIdx;
      }
    }
    out << "\n";
  }
  out.close();
}

TrigMesh SubSet(const TrigMesh& m, const std::vector<size_t>& trigs)
{
  TrigMesh out;
  for (size_t i = 0; i < trigs.size(); i++) {
    unsigned tIdx = trigs[i];
    for (unsigned j = 0; j < 3; j++) {
      for (unsigned d = 0; d < 3; d++) {
        unsigned localv = 3 * tIdx + j;
        unsigned vIdx = m.t[localv];
        out.v.push_back(m.v[ 3*vIdx + d]);
      }
      out.t.push_back(3 * i + j);
    }
  }
  return out;
}

TrigMesh MakeCube(const Vec3f & mn, const Vec3f & mx)
{
  ///@brief cube [0,1]^3
  float CUBE_VERT[8][3] = {
    {0, 0, 0},
    {1, 0, 0},
    {1, 1, 0},
    {0, 1, 0},
    {0, 0, 1},
    {1, 0, 1},
    {1, 1, 1},
    {0, 1, 1}
  };

  unsigned CUBE_TRIG[12][3] = {
    {0, 3, 1},
    {1, 3, 2},  //z=0face
    {7, 4, 5 },
    {7, 5, 6 },  //z=1 face
    {5, 4, 0},
    {5, 0, 1},  //y=0 face
    {3, 6, 2},
    {3, 7, 6},  //y=1 face
    {4, 3, 0},
    {4, 7, 3},  //x=0 face
    { 6, 5, 1 },
    { 1, 2, 6 },  //x=1 face
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
