#include "ObjLoader.h"
#include "TriangulateContour.h"
#include "Vec2.h"
#include "Vec3.h"
#include <fstream>
#include <iostream>
#include <sstream>

/// @brief 
/// @param v 
/// @param polygon 
/// @return list of triplets of indices into polygon array
/// for triangles.
std::vector<unsigned> SplitIntoTriangles(const std::vector<Vec3f>& v) {
  if (v.size() <= 9) { return {0,1,2}; }
  unsigned numVerts = v.size();
  unsigned numTrigs = numVerts - 2;
  std::vector<unsigned> trigs(3 * numTrigs);

  // compute normal;
  // C: a random center vertex on the same plane.
  Vec3f C = v[0];
  for (size_t vi = 0; vi < v.size(); vi ++) {    
    C += v[vi];
  }
  C = (1.0f / numVerts) * C;
  Vec3f N(0, 0, 0);
  for (size_t vi = 0; vi < v.size(); vi++) {
    Vec3f vert0 = v[vi];
    Vec3f vert1;
    if (vi > v.size() - 2) {
      vert1 = v[0];
    }
    else {
      vert1 = v[vi + 1];
    }
    N += (vert0 - C).cross(vert1 - C);
  }
  float norm = N.norm();
  if (norm > 0) {
    N = (1.0f / norm) * N;

    std::vector<Vec2f> verts;
    
    //pick two random axis perp to the normal vector;
    Vec3f xAxis(1, 0, 0);
    if (std::abs(xAxis.dot(N)) > 0.99) {
      xAxis = Vec3f(0, 1, 0);
    }
    Vec3f yAxis = N.cross(xAxis);
    yAxis.normalize();
    xAxis = yAxis.cross(N);

    //project to 2d points in the plane
    for (size_t vi = 0; vi < v.size(); vi++) {
      Vec3f local = v[vi] - C;
      verts[vi] = Vec2f(local.dot(xAxis), local.dot(yAxis));
    }
    TrigMesh patch = TriangulateLoop((const float*)verts.data(), verts.size());
    return patch.t;
  }
  else {
    //whatever
    for (unsigned i = 0; i < numTrigs; i++) {
      trigs.push_back(0);
      trigs.push_back(i + 1);
      trigs.push_back(i + 2);
    }
    return trigs;
  }
  return trigs;
}

std::vector<Vec3f> CopyVertices(const std::vector<float>& meshv,
  const std::vector<unsigned>& indices) {
  std::vector<Vec3f> v(indices.size()); 
  for (size_t i = 0; i < indices.size(); i++) {
    size_t vi = 3 * indices[i];
    v[i] = Vec3f(meshv[vi], meshv[vi + 1], meshv[vi + 2]);
  }
  return v;
}

int LoadOBJFile(std::string filename, std::vector<uint32_t>& t,
                std::vector<float>& v, std::vector<Vec2f>& uv) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    return -1;
  }
  t.clear();
  v.clear();
  uv.clear();

  // Pre-allocate to reduce reallocations
  t.reserve(100000);
  v.reserve(30000);

  std::string curline;
  std::vector<unsigned> indices;
  indices.reserve(16);

  while (std::getline(file, curline)) {
    if (curline.empty()) continue;

    const char* ptr = curline.c_str();

    // Skip leading whitespace
    while (*ptr == ' ' || *ptr == '\t') ++ptr;

    if (*ptr == 'v' && (ptr[1] == ' ' || ptr[1] == '\t')) {
      // Vertex line
      ptr += 2;
      float x, y, z;
      x = strtof(ptr, (char**)&ptr);
      y = strtof(ptr, (char**)&ptr);
      z = strtof(ptr, (char**)&ptr);
      v.push_back(x);
      v.push_back(y);
      v.push_back(z);

    } else if (*ptr == 'f' && (ptr[1] == ' ' || ptr[1] == '\t')) {
      // Face line
      ptr += 2;
      indices.clear();

      while (*ptr) {
        // Skip whitespace
        while (*ptr == ' ' || *ptr == '\t') ++ptr;
        if (*ptr == '\0') break;

        // Parse vertex index (ignore uv/normal indices)
        long idx = strtol(ptr, (char**)&ptr, 10);
        if (idx > 0) {
          indices.push_back(idx - 1);
        }

        // Skip to next space or end (ignore /uv/normal)
        while (*ptr && *ptr != ' ' && *ptr != '\t') ++ptr;
      }

      if (indices.size() < 3) {
        continue;
      }

      if (indices.size() == 3) {
        t.push_back(indices[0]);
        t.push_back(indices[1]);
        t.push_back(indices[2]);
      } else {
        // Non-triangle polygon - triangulate
        std::vector<Vec3f> vCopy = CopyVertices(v, indices);
        std::vector<unsigned> localIndices = SplitIntoTriangles(vCopy);
        for (size_t i = 0; i < localIndices.size(); i++) {
          t.push_back(indices[localIndices[i]]);
        }
      }
    }
    // Skip all other lines (vt, vn, materials, etc.)
  }

  return 0;
}