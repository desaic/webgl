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

  void loadStl(const std::string& filename);
};
