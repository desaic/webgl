#pragma once

#include <string>
#include <vector>

#include "Vec2.h"
#include "Vec3.h"

struct Edge {
  unsigned v[2];
  // sorts the indice increasing order
  Edge(unsigned v0, unsigned v1) {
    if (v0 <= v1) {
      v[0] = v0;
      v[1] = v1;
    } else {
      v[0] = v1;
      v[1] = v0;
    }
  }
  bool operator<(const Edge& o) const {
    if (v[0] < o.v[0]) {
      return true;
    }
    return (v[0] == o.v[0] && v[1] < o.v[1]);
  }
};

struct Triangle {
  Vec3f v[3];
};

class TrigMesh {
 public:
  TrigMesh() = default;
  using TValue = float;
  using TIndex = unsigned;
  std::vector<TValue> v;
  std::vector<TIndex> t;

  // these are all needed just for SDF
  /// triangle edges
  /// Triangle edges are stored in order so that
  /// each edge ei starts with vertex vi
  std::vector<TIndex> te;

  /// normal of each triangle
  std::vector<TValue> nt;

  /// averaged edge normal
  std::vector<Vec3f> ne;

  /// angle weighted vertex normal
  std::vector<Vec3f> nv;

  // texture coordinates
  std::vector<Vec2f> uv;

  size_t GetNumTrigs() const { return t.size() / 3; }
  size_t GetNumVerts() const { return v.size() / 3; }
  int LoadStl(const std::string& filename);
  int LoadObj(const std::string& filename);
  // can get face, edge or vertex normal depending on bary centric coord.
  Vec3f GetNormal(unsigned tIdx, const Vec3f& bary) const;
  // compute triangle area.
  float GetArea(unsigned tIdx) const;
  Vec3f GetRandomSample(const unsigned tIdx, const float r1,
                        const float r2) const;
  // also returns trig normal and a tangent vector
  void GetRandomSample(const unsigned tIdx, const float r1, const float r2,
                       Vec3f& pos, Vec3f& normal, Vec3f& tangent) const;

  void ComputeTrigNormals();
  void ComputeVertNormals();
  /// experimental feature. do not use.
  void ComputePseudoNormals();
  void ComputeTrigEdges();

  unsigned GetIndex(size_t tIdx, unsigned jIdx) const;
  Vec3f GetVertex(size_t vIdx) const;
  Vec3f GetTriangleVertex(size_t tIdx, unsigned jIdx) const;
  Triangle GetTriangleVerts(size_t tIdx) const;
  Vec3f GetTrigNormal(size_t tIdx) const;
  
  void scale(float s);
  void translate(float dx, float dy, float dz);
  void append(const TrigMesh& m);

  void SaveObj(const std::string& filename);
  // save out a point cloud as obj.
  static void SaveObj(const std::string& filename,
                      const std::vector<Vec3f>& pts);
};

TrigMesh SubSet(const TrigMesh& m, const std::vector<size_t>& trigs);
TrigMesh MakeCube(const Vec3f& mn, const Vec3f& mx);
