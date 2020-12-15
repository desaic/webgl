#pragma once

#include <string>
#include <vector>

class TrigMesh
{
public:
  TrigMesh() = default;
  using TValue = float;
  using TIndex = unsigned;
  std::vector<TValue> verts, normals;
  std::vector<TIndex> trigs;

  size_t GetNumTrigs() const { return trigs.size() / 3; }
  size_t GetNumVerts() const { return verts.size() / 3; }
  int LoadStl(const std::string& filename);
};
