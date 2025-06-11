
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "Array3DUtils.h"
#include "BBox.h"

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

int main(int argc, char** argv) {  
  WireMesh wireMesh = LoadWireFile("F:/meshes/wireframe_lattice/wire.txt");

  return 0;
}
