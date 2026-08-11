#pragma once

#include "BBox.h"
#include "Array3D.h"
#include "Matrix3f.h"
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
    /// DDA march through grid cells along the ray, testing triangles in each
    /// cell. origin and dir are in the grid's local frame, dir must be
    /// normalized. Updates t (local-space distance) and returns true if a
    /// closer hit than the incoming t is found.
    bool RayHit(const Vec3f &origin, const Vec3f &dir, float maxT, float &t) const;
    bool InRange(const Vec3f &pt, float margin) const {
      return pt[0] >= origin[0] - margin && pt[0] <= box.vmax[0] + margin &&
             pt[1] >= origin[1] - margin && pt[1] <= box.vmax[1] + margin &&
             pt[2] >= origin[2] - margin && pt[2] <= box.vmax[2] + margin;
    }
    //dx will be reduced if grid is too large
    static const int MAX_GRID_SIZE = 1000;

    /// bytes held by the voxel grid and the per-cell triangle lists,
    /// including the per-vector header overhead.
    size_t MemoryBytes() const;
    /// number of non-empty cells.
    size_t NumCellLists() const { return tLists.size(); }

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

/// a TrigGrid plus the rigid transform that places it in the world.
///
/// grids are built once per item kind in that item's own local frame and
/// shared by every instance of the kind, so a query transforms the point
/// into grid local space instead of the grid being rebuilt per instance.
/// that trades one 3x3 multiply and two adds per query for what used to be
/// a full transformed mesh copy plus a grid per instance.
struct GridInstance {
    const TrigGrid *grid = nullptr;
    // world = rot * (scale * local) + pos.
    Matrix3f rot;
    // rot transposed, precomputed so a query does not transpose per point.
    Matrix3f rotInv;
    Vec3f pos;
    float scale = 1.0f;
    float invScale = 1.0f;
    // true when the grid is already in world space, i.e. the container
    // grids. skips the transform entirely.
    bool worldSpace = true;

    GridInstance() : rot(Matrix3f::identity()), rotInv(Matrix3f::identity()) {}

    /// container grids are built in world space already.
    static GridInstance World(const TrigGrid *g) {
      GridInstance gi;
      gi.grid = g;
      gi.worldSpace = true;
      return gi;
    }

    static GridInstance Local(const TrigGrid *g, const Matrix3f &r,
                              const Vec3f &p, float s) {
      GridInstance gi;
      gi.grid = g;
      gi.rot = r;
      gi.rotInv = r.transposed();
      gi.pos = p;
      gi.scale = s;
      gi.invScale = (s != 0.0f) ? (1.0f / s) : 1.0f;
      gi.worldSpace = false;
      return gi;
    }

    Vec3f ToLocal(const Vec3f &worldPt) const {
      return invScale * (rotInv * (worldPt - pos));
    }
    Vec3f ToWorldPoint(const Vec3f &localPt) const {
      return rot * (scale * localPt) + pos;
    }
    Vec3f ToWorldDir(const Vec3f &localDir) const { return rot * localDir; }
};
