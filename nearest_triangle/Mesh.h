#pragma once

#include <cstdint>
#include <vector>

// container for passing and returning mesh data to API.
// pointers are always preallocated by caller.
struct MeshPtrs {
    const float *vertices = nullptr;
    unsigned numVerts = 0;
    const unsigned *faces = nullptr;
    unsigned numFaces = 0;
};

// mesh container for implementation uses.
class Mesh {
  public:
    std::vector<float> vertices;
    std::vector<unsigned> triangles;
    size_t numVertices() const {
      return vertices.size() / 3;
    }
    size_t numTriangles() const {
      return triangles.size() / 3;
    }
};
