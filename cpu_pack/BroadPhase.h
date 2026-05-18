#include "BBox.h"
#include <algorithm>

static float clampf(float val, float lb, float ub) {
  if (val < lb)
    return lb;
  if (val > ub)
    return ub;
  return val;
}

class UniformGridBroadPhase {
  private:
    Box3f m_worldBounds;
    int m_dimX, m_dimY, m_dimZ;
    float m_cellWidthX, m_cellWidthY, m_cellWidthZ; // Changed to float

    std::vector<std::vector<unsigned>> m_cells;

    // Modified to accept Box3d but compute in float precision
    void PosToCell(const Vec3f &pos, int &cx, int &cy, int &cz) const {
      Vec3f rel = pos - m_worldBounds.vmin;
      cx = static_cast<int>(rel[0] / m_cellWidthX);
      cy = static_cast<int>(rel[1] / m_cellWidthY);
      cz = static_cast<int>(rel[2] / m_cellWidthZ);

      cx = clampf(cx, 0, m_dimX - 1);
      cy = clampf(cy, 0, m_dimY - 1);
      cz = clampf(cz, 0, m_dimZ - 1);
    }

    inline int GetCellIndex(int cx, int cy, int cz) const {
      return cx + cy * m_dimX + cz * m_dimX * m_dimY;
    }

  public:
    /**
     * @param worldBox Total low-res bounding arena volume.
     * @param dimX, dimY, dimZ Grid resolutions (e.g., 16x16x16 or 32x32x32).
     */
    UniformGridBroadPhase(const Box3f &worldBox, int dimX, int dimY, int dimZ)
        : m_worldBounds(worldBox), m_dimX(dimX), m_dimY(dimY), m_dimZ(dimZ) {
      m_cells.resize(dimX * dimY * dimZ);
      Vec3f worldSize = worldBox.vmax - worldBox.vmin;
      m_cellWidthX = static_cast<float>(worldSize[0]) / dimX;
      m_cellWidthY = static_cast<float>(worldSize[1]) / dimY;
      m_cellWidthZ = static_cast<float>(worldSize[2]) / dimZ;
    }

    // Adds a finalized mesh instance into all grid cells overlapping its AABB bounds
    void Add(const Box3f &box, unsigned instId) {
      int minX, minY, minZ;
      int maxX, maxY, maxZ;

      PosToCell(box.vmin, minX, minY, minZ);
      PosToCell(box.vmax, maxX, maxY, maxZ);

      for (int z = minZ; z <= maxZ; ++z) {
        for (int y = minY; y <= maxY; ++y) {
          for (int x = minX; x <= maxX; ++x) {
            m_cells[GetCellIndex(x, y, z)].push_back(instId);
          }
        }
      }
    }

    /**
     * Queries all unique instances residing inside or near the query box expanded by maxDistance.
     *
     * @param queryBox    The current bounding box of the item being tested.
     * @param maxDistance The inflation cushion/search radius threshold.
     */
    std::vector<unsigned> GetNearby(const Box3f &queryBox, float maxDistance) const {
      std::vector<unsigned> nearbyIds;

      // 1. Inflate the query bounding box dimensions by the padding distance
      Box3f expandedBox = queryBox;
      double dist = static_cast<double>(maxDistance);

      expandedBox.vmin -= Vec3f(dist);
      expandedBox.vmax += Vec3f(dist);

      // 2. Identify the range of grid cells that intersect the expanded volume
      int minX, minY, minZ;
      int maxX, maxY, maxZ;

      PosToCell(expandedBox.vmin, minX, minY, minZ);
      PosToCell(expandedBox.vmax, maxX, maxY, maxZ);

      // 3. Scan the grid region and accumulate candidate instance IDs
      for (int z = minZ; z <= maxZ; ++z) {
        for (int y = minY; y <= maxY; ++y) {
          for (int x = minX; x <= maxX; ++x) {
            const auto &cellInstances = m_cells[GetCellIndex(x, y, z)];
            nearbyIds.insert(nearbyIds.end(), cellInstances.begin(), cellInstances.end());
          }
        }
      }

      // 4. Fast deduplication: clean out multiple cell overlap redundancies
      std::sort(nearbyIds.begin(), nearbyIds.end());
      nearbyIds.erase(std::unique(nearbyIds.begin(), nearbyIds.end()), nearbyIds.end());

      return nearbyIds;
    }

    // Clears the entire grid structure to reset state between fresh packing trials
    void Clear() {
      for (auto &cell : m_cells) {
        cell.clear();
      }
    }
};
