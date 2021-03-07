#include "TrigMesh.h"
#include "stl_reader.h"

#include <iostream>
#include <map>

int TrigMesh::LoadStl(const std::string& meshFile) {
  std::vector<unsigned > solids;
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
  return 0;
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
