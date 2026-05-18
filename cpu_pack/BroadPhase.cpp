#include "BroadPhase.h"

static float clampf(float val, float lb, float ub) {
  if (val < lb)
    return lb;
  if (val > ub)
    return ub;
  return val;
}

void BroadPhaseGrid::PosToCell(const Vec3f &pos, int &cx, int &cy, int &cz) const {
  Vec3f rel = pos - m_box.vmin;
  cx = int(rel[0] / m_dx);
  cy = int(rel[1] / m_dx);
  cz = int(rel[2] / m_dx);

  Vec3u size = m_cells.GetSize();
  cx = clampf(cx, 0, size[0] - 1);
  cy = clampf(cy, 0, size[1] - 1);
  cz = clampf(cz, 0, size[2] - 1);
}

void BroadPhaseGrid::Add(const Box3f &box, unsigned instId) {
  int minX, minY, minZ;
  int maxX, maxY, maxZ;

  PosToCell(box.vmin, minX, minY, minZ);
  PosToCell(box.vmax, maxX, maxY, maxZ);

  for (int z = minZ; z <= maxZ; ++z) {
    for (int y = minY; y <= maxY; ++y) {
      for (int x = minX; x <= maxX; ++x) {
        m_cells(x, y, z).push_back(instId);
      }
    }
  }
}

std::vector<unsigned> BroadPhaseGrid::GetNearby(const Box3f &queryBox, float maxDistance) const {
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
        const auto &cellInstances = m_cells(x, y, z);
        nearbyIds.insert(nearbyIds.end(), cellInstances.begin(), cellInstances.end());
      }
    }
  }

  // 4. Fast deduplication: clean out multiple cell overlap redundancies
  std::sort(nearbyIds.begin(), nearbyIds.end());
  nearbyIds.erase(std::unique(nearbyIds.begin(), nearbyIds.end()), nearbyIds.end());

  return nearbyIds;
}
