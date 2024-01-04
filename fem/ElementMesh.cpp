#include "ElementMesh.h"
#include "femError.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <map>

int fem_error = 0;

double ElementMesh::GetElasticEnergyEle(int eId) const {
  const auto& mat = m[eId];
  double ene = mat.GetEnergy(eId, *this);
  return ene;
}

double ElementMesh::GetElasticEnergy() const {
  double ene = 0;
  for (size_t i = 0; i < e.size(); i++) {
    ene += GetElasticEnergyEle(i);
  }
  return ene;
}

std::vector<Vec3f> ElementMesh::GetForceEle(int eId) const {
  const auto& mat = m[eId];
  std::vector<Vec3f> force= mat.GetForce(eId, *this);
  return force;
}

std::vector<Vec3f> ElementMesh::GetForce() const {
  std::vector<Vec3f> force(X.size(), Vec3f(0,0,0));
  for (size_t i = 0; i < e.size(); i++) {
    std::vector<Vec3f> eleForce = GetForceEle(i);
    for (size_t j = 0; j < eleForce.size(); j++) {
      size_t vertIdx = (*e[i])[j];
      force[vertIdx] += eleForce[j];
    }
  }
  return force;
}

void ElementMesh::InitElements() {
  if (m.size() != e.size()) {
    m.resize(e.size());
  }
  for (size_t i = 0; i < e.size(); i++) {
    e[i]->InitJacobian(m[i].quadType, X);
  }
}

void ElementMesh::InitStiffnessPattern() {
  edgeToSparse.clear();      
  // for each vertex, map from a neighboring vertex to index in vals array in 
  // sparse mat.
  edgeToSparse.resize(X.size());
  size_t edgeCount = 0;
  for (size_t i = 0; i < e.size(); i++) {
    for (size_t j = 0; j < e[i]->NumVerts(); j++) {
      unsigned vj = (*e[i])[j];
      std::map<unsigned, size_t>& verts = edgeToSparse[vj];
      for (size_t k = 0; k < e[i]->NumVerts(); k++) {
        unsigned vk = (*e[i])[k];
        auto it = verts.find(vk);
        if (it == verts.end()) {
          verts[vk] = edgeCount;
          edgeCount++;        
        }
      }
    }
  }

  std::cout << "edge count " << edgeCount << "\n";

  size_t numCols = 3 * X.size(); 
  size_t nnz = 9 * edgeCount;
  //square matrix
  K.Allocate(numCols, numCols, 9 * edgeCount);
  
  K.colStart[0] = 0;
  for (size_t i = 0; i < edgeToSparse.size(); i++) {
    std::map<unsigned, size_t>& verts = edgeToSparse[i];
    for (unsigned d = 0; d < 3; d++) {
      unsigned col = 3 * i + d;
      unsigned colStart = K.colStart[col];
      K.colStart[col + 1] = K.colStart[col] + 3 * verts.size();
      unsigned count = 0;
      for (auto it : edgeToSparse[i]) {
        K.rowIdx[colStart + count] = 3 * (it.second);
        K.rowIdx[colStart + count + 1] = 3 * (it.second) + 1;
        K.rowIdx[colStart + count + 2] = 3 * (it.second) + 2;
        count+=3;
      }
    }
  }
}

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

  InitElements();
}

void ElementMesh::SaveTxt(const std::string& file) {
  std::ofstream out(file);
  size_t numVerts = X.size();
  size_t numElements = e.size();
  out << "verts " << numVerts << "\n";
  out << "elts " << numElements << "\n";
  for (size_t i = 0; i < numVerts; i++) {
    out << X[i][0] << " " << X[i][1] << " " << X[i][2] << "\n";
  }
  for (size_t i = 0; i < numElements; i++) {
    unsigned nV = e[i]->NumVerts();
    out << nV;
    for (size_t j = 0; j < nV; j++) {
      out << " " << (*e[i])[j];
    }
    out << "\n";
  }
}
