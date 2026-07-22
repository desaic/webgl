#pragma once

#include "BBox.h"
#include "Array3D.h"
#include "Vec3.h"
#include <cstdint>
#include <vector>

class TrigMesh;

struct ContactInfo {
    float dist = 1e30f;
    Vec3f normal;
    Vec3f closestPt;
};

class TrigGrid {
  public:
    void Build(const TrigMesh &m, float voxSize);
    float NearestTriangle(const Vec3f& point, float maxDist, Vec3f& normal) const;
    /// @brief distance is always positive. negative distance is invalid.
    /// @param point 
    /// @param maxDist 
    /// @return 
    ContactInfo NearestTriangle(const Vec3f &point, float maxDist) const ;
    bool InRange(const Vec3f &pt, float margin) const {
      return pt[0] >= origin[0] - margin && pt[0] <= box.vmax[0] + margin &&
             pt[1] >= origin[1] - margin && pt[1] <= box.vmax[1] + margin &&
             pt[2] >= origin[2] - margin && pt[2] <= box.vmax[2] + margin;
    }
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
