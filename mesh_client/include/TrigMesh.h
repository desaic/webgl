#pragma once

#include <string>
#include <vector>

class TrigMesh
{
public:
  TrigMesh() = default;
  using TValue = float;
  using TIndex = unsigned;
  std::vector<TValue> v, n;
  std::vector<TIndex> t;

  size_t GetNumTrigs() const { return t.size() / 3; }
  size_t GetNumVerts() const { return v.size() / 3; }
  int LoadStl(const std::string& filename);

  void scale(float s);
  void translate(float dx, float dy, float dz);
  void append(const TrigMesh& m);
};
