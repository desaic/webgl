#include "TrigGrid.h"
#include "Profiler.h"
#include "TrigMesh.h"
#include "cpu_voxelizer.h"

#include <cmath>

float dot(const Vec3f &a, const Vec3f &b) {
  return a.dot(b);
}

Vec3f closestPointTriangle(const Vec3f &p, const Vec3f &a, const Vec3f &b, const Vec3f &c) {
  const Vec3f ab = b - a;
  const Vec3f ac = c - a;
  const Vec3f ap = p - a;

  const float d1 = dot(ab, ap);
  const float d2 = dot(ac, ap);
  if (d1 <= 0.f && d2 <= 0.f)
    return a; // #1

  const Vec3f bp = p - b;
  const float d3 = dot(ab, bp);
  const float d4 = dot(ac, bp);
  if (d3 >= 0.f && d4 <= d3)
    return b; // #2

  const Vec3f cp = p - c;
  const float d5 = dot(ab, cp);
  const float d6 = dot(ac, cp);
  if (d6 >= 0.f && d5 <= d6)
    return c; // #3

  const float vc = d1 * d4 - d3 * d2;
  if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
    const float v = d1 / (d1 - d3);
    return a + v * ab; // #4
  }

  const float vb = d5 * d2 - d1 * d6;
  if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
    const float v = d2 / (d2 - d6);
    return a + v * ac; // #5
  }

  const float va = d3 * d6 - d5 * d4;
  if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
    const float v = (d4 - d3) / ((d4 - d3) + (d5 - d6));
    return b + v * (c - b); // #6
  }

  const float denom = 1.f / (va + vb + vc);
  const float v = vb * denom;
  const float w = vc * denom;
  return a + v * ab + w * ac; // #0
}

void TrigGrid::Build(const TrigMesh &m, float voxSize) {
  mesh = &m;
  voxelSize = voxSize;
  box = ComputeBBox(mesh->v);
  Vec3f boxSize = box.vmax - box.vmin;

  VoxConf conf;
  conf.origin = {box.vmin[0], box.vmin[1], box.vmin[2]};
  conf.unit = {voxelSize, voxelSize, voxelSize};
  Vec3u size;
  for (unsigned d = 0; d < 3; d++) {
    size[d] = unsigned(boxSize[d] / voxelSize) + 2;    
  }

  conf.gridSize = size;
  origin = box.vmin;

  grid.Allocate(conf.gridSize, 0);
  // tLists[0] is reserved for empty voxels
  tLists.push_back({});
  cpu_voxelize_mesh_cb(conf, mesh, [&](unsigned int x, unsigned int y, unsigned int z, unsigned int tIdx) {
    uint32_t &gridVal = grid(x, y, z);
    if (gridVal == 0) {
      // First triangle for this voxel - create new list
      tLists.push_back({tIdx});
      gridVal = tLists.size() - 1;
    } else {
      // Voxel already has triangles - append to existing list
      tLists[gridVal].push_back(tIdx);
    }
  });
}

size_t TrigGrid::MemoryBytes() const {
  size_t bytes = grid.GetData().size() * sizeof(uint32_t);
  bytes += tLists.capacity() * sizeof(std::vector<unsigned>);
  for (const auto &list : tLists) {
    bytes += list.capacity() * sizeof(unsigned);
  }
  return bytes;
}

// 0, -1, +1, -2, +2, ... so a cell walk starts at the centre and works
// outward, which lets minDist shrink before the far cells are tested.
static inline int CenterOutOffset(int i) {
  return (i % 2 == 0) ? (i / 2) : -((i + 1) / 2);
}

// distance from p to the interval [lo, hi], 0 if inside.
static inline float AxisGap(float p, float lo, float hi) {
  if (p < lo) {
    return lo - p;
  }
  if (p > hi) {
    return p - hi;
  }
  return 0.0f;
}

ContactInfo TrigGrid::NearestTriangle(const Vec3f &point, float maxDist) const{
// Convert point to grid coordinates
  Vec3f gridCoord = (point - origin) * (1.0f / voxelSize);
  Vec3i gridIdx = Vec3i(int(gridCoord[0]), int(gridCoord[1]), int(gridCoord[2]));
  ContactInfo info;
  float minDist = maxDist;
  Vec3u gridSize = grid.GetSize();
  unsigned long cellsScanned = 0, trigTests = 0;
  // Walk the cells around the query point, skipping any cell whose box is
  // already farther than the best distance so far. maxDist is the sample
  // spacing (0.22 cm for a berry) while a cell is voxelSize (1 cm) across, so the
  // plain 27-cell walk was scanning a 3 cm cube to answer a 0.22 cm
  // question. culling is exact: a triangle whose nearest point lies in a
  // culled cell is farther than minDist by construction, and the voxelizer
  // registers a triangle in every cell it overlaps, so a triangle reaching
  // into a kept cell is still found through that cell.
  //
  // the center cell comes first so minDist shrinks before the neighbors are
  // tested, which culls more of them.
  //
  // the window is sized from maxDist rather than fixed at 3x3x3: a cell
  // holds triangles that overlap it, so a triangle within maxDist of the
  // query can sit up to ceil(maxDist / voxelSize) cells away. at
  // maxDist <= voxelSize this is the same 3x3x3 as before, and it stays
  // correct if the grid is ever built finer than the query radius.
  int reach = int(std::ceil(maxDist / voxelSize));
  if (reach < 1) {
    reach = 1;
  }
  int span = 2 * reach + 1;
  for (int iz = 0; iz < span; iz++) {
    int gz = gridIdx[2] + CenterOutOffset(iz);
    if (gz < 0 || gz >= int(gridSize[2]))
      continue;
    float loZ = origin[2] + float(gz) * voxelSize;
    float gapZ = AxisGap(point[2], loZ, loZ + voxelSize);
    float d2Z = gapZ * gapZ;
    if (d2Z >= minDist * minDist)
      continue;

    for (int iy = 0; iy < span; iy++) {
      int gy = gridIdx[1] + CenterOutOffset(iy);
      if (gy < 0 || gy >= int(gridSize[1]))
        continue;
      float loY = origin[1] + float(gy) * voxelSize;
      float gapY = AxisGap(point[1], loY, loY + voxelSize);
      float d2ZY = d2Z + gapY * gapY;
      if (d2ZY >= minDist * minDist)
        continue;

      for (int ix = 0; ix < span; ix++) {
        int gx = gridIdx[0] + CenterOutOffset(ix);
        if (gx < 0 || gx >= int(gridSize[0]))
          continue;
        float loX = origin[0] + float(gx) * voxelSize;
        float gapX = AxisGap(point[0], loX, loX + voxelSize);
        if (d2ZY + gapX * gapX >= minDist * minDist)
          continue;

        uint32_t listIdx = grid(gx, gy, gz);
        if (listIdx == 0)
          continue; // Empty voxel

        const std::vector<unsigned> &trigList = tLists[listIdx];
        cellsScanned++;
        trigTests += trigList.size();

        // Check distance to each triangle in this voxel
        for (unsigned tIdx : trigList) {
          Vec3f a, b, c;
          size_t vIdx0 = mesh->t[3 * tIdx];
          size_t vIdx1 = mesh->t[3 * tIdx + 1];
          size_t vIdx2 = mesh->t[3 * tIdx + 2];
          a = Vec3f(mesh->v[3 * vIdx0], mesh->v[3 * vIdx0 + 1], mesh->v[3 * vIdx0 + 2]);
          b = Vec3f(mesh->v[3 * vIdx1], mesh->v[3 * vIdx1 + 1], mesh->v[3 * vIdx1 + 2]);
          c = Vec3f(mesh->v[3 * vIdx2], mesh->v[3 * vIdx2 + 1], mesh->v[3 * vIdx2 + 2]);

          Vec3f closestPt = closestPointTriangle(point, a, b, c);
          float dist = (point - closestPt).norm();
          if (dist < minDist) {
            minDist = dist;
            // was assigned for every triangle, so the returned point came
            // from the last one tested rather than the nearest. the caller
            // takes the sign of the contact from it, so a mismatched point
            // and normal could flip a contact's sign.
            info.closestPt = closestPt;
            // Compute triangle normal
            Vec3f ab = b - a;
            Vec3f ac = c - a;
            Vec3f normal = ab.cross(ac);
            float normLen = normal.norm();
            if (normLen > 0.0f) {
              normal = normal * (1.0f / normLen);
            }
            info.normal = normal;
          }
        }
      }
    }
  }
  PROFILE_COUNT("count.grid_cells_scanned", cellsScanned);
  PROFILE_COUNT("count.grid_trig_tests", trigTests);
  info.dist = minDist;
  return info;
}

float TrigGrid::NearestTriangle(const Vec3f &point, float maxDist, Vec3f& normal) const {
  ContactInfo info = NearestTriangle(point, maxDist);
  if(info.dist < maxDist){
    normal = info.normal;
  }
  return info.dist;
}
