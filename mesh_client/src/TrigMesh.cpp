#include "TrigMesh.h"
#include "stl_reader.h"

#include <iostream>

int TrigMesh::LoadStl(const std::string& meshFile) {
  std::vector<unsigned > solids;
  bool success = false;
  try {
    success = stl_reader::ReadStlFile(meshFile.c_str(), verts, normals, trigs, solids);
  }
  catch (...) {
  }
  if (!success) {
    std::cout << "error reading stl file " << meshFile << "\n";
    return -1;
  }
  return 0;
}
