#include "ElementMesh.h"
#include <fstream>
#include <iostream>
#include <sstream>

void ElementMesh::LoadTxt(const std::string& file) {
  std::ifstream in(file);
  std::string line;
  size_t numVerts = 0;
  size_t numElements = 0;
  std::string token;
  in >> token >> numVerts;
  in >> token >> numElements;
  X.resize(numVerts);
  e.clear();
  for (size_t i = 0; i < numVerts; i++) {
    for (unsigned dim = 0; dim < 3; dim++) {
      in >> X[i][dim];
    }
  }
  x = X;
  for (size_t i = 0; i < numElements; i++) {
    bool hasLine = false;
    while (in.good()) {
      std::getline(in, line);
      if (line.size() > 3) {
        hasLine = true;
        break;
      }
    }
    if (!hasLine) {
      break;
    }

    std::istringstream iss(line);
    unsigned numVerts = 0;
    iss >> numVerts;
    std::unique_ptr<Element> ePtr;
    if (numVerts == 4) {
      ePtr = std::make_unique<TetElement>();
    } else if (numVerts == 8) {
      ePtr = std::make_unique<HexElement>();
    } else {
      std::cout << "unknown element type " << numVerts << "verts\n";
      continue;
    }
    for (unsigned j = 0; j < numVerts; j++) {
      iss >> (*ePtr)[j];
    }
    e.push_back(std::move(ePtr));
  }
  if (e.size() != numElements) {
    std::cout << "only found " << e.size() << " out of " << numElements
              << " elements\n";
  }
}