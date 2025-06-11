
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <map>

#include "Array2D.h"
#include "Array3D.h"
#include "Array3DUtils.h"
#include "BBox.h"
#include "TrigMesh.h"

struct Vertex {
  Vec3f pos;
  float diam= 1.0f;
};

struct LineSeg {
  size_t i0= 0, i1=0;
};

struct WireMesh{
  std::vector<Vertex> v;
  std::vector<LineSeg> l;
};

WireMesh LoadWireFile(const std::string& filename) {
  WireMesh wireMesh;
  std::ifstream in(filename);
  std::string line;
  std::getline(in, line);
  std::istringstream ss(line);
  std::string tok;
  size_t numVerts = 0, numLines = 0;
  ss >> tok;
  ss >> numVerts;
  std::cout << "loading " << numVerts << " vertices\n";
  wireMesh.v.resize(numVerts);
  for (size_t i = 0; i < wireMesh.v.size(); i++) {
    Vec3f p;
    in >> p[0] >> p[1] >> p[2];
    wireMesh.v[i].pos = p;
  }
  
  //skip an empty line
  std::getline(in, line);
  if (line.size() < 3) {
    std::getline(in, line);
  }
  ss = std::istringstream(line);
  size_t numDiameters = 0;
  ss >> tok >> numDiameters;
  if (numDiameters != numVerts) {
    std::cout << "Warning: number of diameters "
              << numDiameters << " differs from number of vertices " << numVerts
              << "\n";
    numDiameters = std::min(numDiameters, numVerts);    
  }
  for (size_t i = 0; i < numDiameters; i++) {
    in >> wireMesh.v[i].diam;
  }

  std::getline(in, line);
  if (line.size() < 3) {
    std::getline(in, line);
  }
  ss = std::istringstream(line);
  ss >> tok >> numLines;
  wireMesh.l.resize(numLines);
  for (size_t i = 0; i < numLines; i++) {
    size_t v0, v1;
    in>>v0>>v1;
    if (v0 >= numVerts || v1 >= numVerts) {
      std::cout << "vertex index out of bounds " << v0 << " " << v1 << "\n";
      continue;
    }
    wireMesh.l[i].i0 = v0;
    wireMesh.l[i].i1 = v1;
  }
  return wireMesh;
}

using Lookup=std::map<std::pair<unsigned, unsigned>, unsigned>;
unsigned vertex_for_edge(Lookup& lookup, std::vector<Vec3f>& vertices,
                         unsigned first, unsigned second) {
  Lookup::key_type key(first, second);
  if (key.first > key.second) std::swap(key.first, key.second);

  auto inserted = lookup.insert({key, vertices.size()});
  if (inserted.second) {
    auto& edge0 = vertices[first];
    auto& edge1 = vertices[second];
    auto point = (edge0 + edge1);
    point.normalize();
    vertices.push_back(point);
  }

  return inserted.first->second;
}
std::vector<Vec3u> subdivide(std::vector<Vec3f>& vertices,
                             std::vector<Vec3u> triangles) {
  Lookup lookup;
  std::vector<Vec3u> result;

  for (auto&& each : triangles) {
    std::array<unsigned, 3> mid;
    for (int edge = 0; edge < 3; ++edge) {
      mid[edge] = vertex_for_edge(lookup, vertices, each[edge],
                                  each[(edge + 1) % 3]);
    }

    result.push_back({each[0], mid[0], mid[2]});
    result.push_back({each[1], mid[1], mid[0]});
    result.push_back({each[2], mid[2], mid[1]});
    result.push_back({mid[0], mid[1], mid[2]});
  }

  return result;
}

void FlipFaceOrientation(TrigMesh& m) {
  for (size_t t = 0; t < m.GetNumTrigs(); t++) {
    unsigned tmp = m.t[3 * t];
    m.t[3 * t] = m.t[3 * t + 1];
    m.t[3 * t + 1] = tmp;
  }
}

//makes a ico sphere with diameter 1.
TrigMesh MakeSphere(int subdivisions) {
  if (subdivisions < 1) {
    subdivisions = 1;
  }
  if (subdivisions > 5) {
    subdivisions = 5;
  }
  const float X = .525731112119133606f;
  const float Z = .850650808352039932f;
  const float N = 0.f;
  TrigMesh m;
  std::vector<Vec3f> vertices = {{-X, N, Z}, {X, N, Z},   {-X, N, -Z},
                                 {X, N, -Z}, {N, Z, X},   {N, Z, -X},
                                 {N, -Z, X}, {N, -Z, -X}, {Z, X, N},
                                 {-Z, X, N}, {Z, -X, N},  {-Z, -X, N}};

  std::vector<Vec3u> triangles = {
      {0, 4, 1},  {0, 9, 4},  {9, 5, 4},  {4, 5, 8},  {4, 8, 1},
      {8, 10, 1}, {8, 3, 10}, {5, 3, 8},  {5, 2, 3},  {2, 7, 3},
      {7, 10, 3}, {7, 6, 10}, {7, 11, 6}, {11, 0, 6}, {0, 1, 6},
      {6, 1, 10}, {9, 0, 11}, {9, 11, 2}, {9, 2, 5},  {7, 2, 11}};

  for (int i = 0; i < subdivisions; ++i) {
    triangles = subdivide(vertices, triangles);
  }
  m.v.resize(vertices.size() * 3);
  m.t.resize(triangles.size() * 3);
  for (size_t i = 0; i < vertices.size(); i++) {
    m.v[3 * i] = vertices[i][0];
    m.v[3 * i+1] = vertices[i][1];
    m.v[3 * i+ 2] = vertices[i][2];
  }
  for (size_t i = 0; i < triangles.size(); i++) {
    m.t[3 * i] = triangles[i][0];
    m.t[3 * i + 1] = triangles[i][1];
    m.t[3 * i + 2] = triangles[i][2];
  }
  FlipFaceOrientation(m);
  m.scale(0.5f);
  return m;
}


//makes a cylinder with 1 diameter and 1 height.
TrigMesh MakeCylinder(unsigned radialVerts) {
  if (radialVerts < 3) {
    radialVerts = 3;
  }
  float radius = 0.5f;
  float height = 1.0f;
  TrigMesh m;
  m.v.resize( 3 * 2 * radialVerts);
  for (unsigned i = 0; i < radialVerts; ++i) {
    float angle = 2.0f * 3.14159265 * i / radialVerts;
    float x = radius * std::cos(angle);
    float y = radius * std::sin(angle);
    m.v[3 * i] = x;
    m.v[3 * i + 1] = y;
    m.v[3 * i + 2] = 0;
    m.v[3 * (i+radialVerts)] = x;
    m.v[3 * (i + radialVerts) + 1] = y;
    m.v[3 * (i + radialVerts) + 2] = height;
  }
  for (unsigned i = 0; i < radialVerts; ++i) {

    m.t.push_back(i);
    m.t.push_back((i + 1) % radialVerts);
    m.t.push_back(i + radialVerts);

    m.t.push_back(i + radialVerts);
    m.t.push_back((i + 1) % radialVerts);
    m.t.push_back((i + 1) % radialVerts + radialVerts);
  
  }

  m.v.push_back(0);
  m.v.push_back(0);
  m.v.push_back(0);
  m.v.push_back(0);
  m.v.push_back(0);
  m.v.push_back(height);
  unsigned botCenter = 2 * radialVerts;
  unsigned topCenter = botCenter + 1;
  for (unsigned i = 0; i < radialVerts; ++i) {
    m.t.push_back(botCenter);
    m.t.push_back((i + 1) % radialVerts);
    m.t.push_back(i);

    m.t.push_back(topCenter);
    m.t.push_back(i + radialVerts);
    m.t.push_back((i + 1) % radialVerts + radialVerts);
  }
  return m;
}

TrigMesh ToCylinders(const WireMesh& wireMesh) {
  TrigMesh m;
  return m;
}

int main(int argc, char** argv) {  
const std::string dataDir = "F:/meshes/wireframe_lattice";
  WireMesh wireMesh = LoadWireFile(dataDir + "/wire.txt");
  TrigMesh sp = MakeSphere(2);
  
  sp.SaveObj(dataDir + "/sphere.obj");

  TrigMesh cy = MakeCylinder(16);
  cy.SaveObj(dataDir + "/cyl.obj");
  return 0;
}
