#pragma once

#include <string>
#include <vector>

class TrigMesh
{
public:
  TrigMesh() = default;
  using TValue = float;
  using TIndex = unsigned;
  std::vector<TValue> v;
  std::vector<TIndex> t;
  ///normal of each triangle
  std::vector<TValue> nt;
  size_t GetNumTrigs() const { return t.size() / 3; }
  size_t GetNumVerts() const { return v.size() / 3; }
  int LoadStl(const std::string& filename);

  void ComputeTrigNormals();

  void scale(float s);
  void translate(float dx, float dy, float dz);
  void append(const TrigMesh& m);
};
