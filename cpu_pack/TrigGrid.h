#pragma once

#include "BBox.h"
#include "Array3D.h"
#include "Vec3.h"
#include <cstdint>
#include <vector>

class TrigMesh;

class TrigGrid {
  public:
    void Build(const TrigMesh &m, float voxSize);
    float NearestTriangle(const Vec3f& point, float maxDist) const;
    //dx will be reduced if grid is too large
    static const int MAX_GRID_SIZE = 1000;

  private:
    Array3D<uint32_t> grid;
    // tlists can be compressed to remove std::vector overhead 24bytes/cell
    std::vector<std::vector<unsigned>> tLists;
    Box3f box;
    Vec3f origin;
    float voxelSize = 1e-2;
    // does not own pointer, set during Build()
    const TrigMesh* mesh = nullptr;
};
