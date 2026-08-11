#include "TrigGrid.h"
#include "Profiler.h"
#include "TrigMesh.h"
#include "cpu_voxelizer.h"

#include <algorithm>
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

static bool RayTriangleIntersect(const Vec3f &origin, const Vec3f &dir,
                                 const Vec3f &v0, const Vec3f &v1,
                                 const Vec3f &v2, float maxT, float &t) {
  Vec3f e1 = v1 - v0;
  Vec3f e2 = v2 - v0;
  Vec3f h = dir.cross(e2);
  float a = e1.dot(h);
  if (std::fabs(a) < 1e-8f) {
    return false;
  }
  float f = 1.0f / a;
  Vec3f s = origin - v0;
  float u = f * s.dot(h);
  if (u < 0.0f || u > 1.0f) {
    return false;
  }
  Vec3f q = s.cross(e1);
  float v = f * dir.dot(q);
  if (v < 0.0f || u + v > 1.0f) {
    return false;
  }
  t = f * e2.dot(q);
  return t > 1e-6f && t <= maxT;
}

bool TrigGrid::RayHit(const Vec3f &ro, const Vec3f &rd, float maxT,
                      float &bestT) const {
  Vec3u gsize = grid.GetSize();
  Vec3f start = ro;

  // If origin is outside the grid box, advance to the entry point via a
  // slab test. The ray may start on the container surface, far from any
  // item, so this is the common case.
  bool outside = ro[0] < box.vmin[0] || ro[0] > box.vmax[0] ||
                 ro[1] < box.vmin[1] || ro[1] > box.vmax[1] ||
                 ro[2] < box.vmin[2] || ro[2] > box.vmax[2];
  if (outside) {
    float t0 = 0.0f;
    float t1 = bestT;
    for (int i = 0; i < 3; i++) {
      if (std::fabs(rd[i]) < 1e-8f) {
        if (ro[i] < box.vmin[i] || ro[i] > box.vmax[i]) {
          return false;
        }
      } else {
        float invD = 1.0f / rd[i];
        float tn = (box.vmin[i] - ro[i]) * invD;
        float tf = (box.vmax[i] - ro[i]) * invD;
        if (invD < 0.0f) {
          std::swap(tn, tf);
        }
        t0 = std::max(t0, tn);
        t1 = std::min(t1, tf);
        if (t0 > t1) {
          return false;
        }
      }
    }
    if (t0 > bestT) {
      return false;
    }
    start = ro + t0 * rd;
  }

  Vec3f fc = (start - origin) * (1.0f / voxelSize);
  int cx = int(std::floor(fc[0]));
  int cy = int(std::floor(fc[1]));
  int cz = int(std::floor(fc[2]));

  if (cx < 0 || cx >= int(gsize[0]) || cy < 0 || cy >= int(gsize[1]) ||
      cz < 0 || cz >= int(gsize[2])) {
    return false;
  }

  int stepX = (rd[0] >= 0.0f) ? 1 : -1;
  int stepY = (rd[1] >= 0.0f) ? 1 : -1;
  int stepZ = (rd[2] >= 0.0f) ? 1 : -1;

  float tDeltaX = (rd[0] != 0.0f) ? voxelSize / std::fabs(rd[0]) : 1e30f;
  float tDeltaY = (rd[1] != 0.0f) ? voxelSize / std::fabs(rd[1]) : 1e30f;
  float tDeltaZ = (rd[2] != 0.0f) ? voxelSize / std::fabs(rd[2]) : 1e30f;

  float tMaxX, tMaxY, tMaxZ;
  if (rd[0] > 0.0f) {
    tMaxX = (float(cx + 1) - fc[0]) * tDeltaX;
  } else if (rd[0] < 0.0f) {
    tMaxX = (fc[0] - float(cx)) * tDeltaX;
  } else {
    tMaxX = 1e30f;
  }
  if (rd[1] > 0.0f) {
    tMaxY = (float(cy + 1) - fc[1]) * tDeltaY;
  } else if (rd[1] < 0.0f) {
    tMaxY = (fc[1] - float(cy)) * tDeltaY;
  } else {
    tMaxY = 1e30f;
  }
  if (rd[2] > 0.0f) {
    tMaxZ = (float(cz + 1) - fc[2]) * tDeltaZ;
  } else if (rd[2] < 0.0f) {
    tMaxZ = (fc[2] - float(cz)) * tDeltaZ;
  } else {
    tMaxZ = 1e30f;
  }

  bool hit = false;
  while (cx >= 0 && cx < int(gsize[0]) && cy >= 0 && cy < int(gsize[1]) &&
         cz >= 0 && cz < int(gsize[2])) {
    uint32_t listIdx = grid(cx, cy, cz);
    if (listIdx != 0) {
      const std::vector<unsigned> &trigList = tLists[listIdx];
      for (unsigned tIdx : trigList) {
        Vec3f v0 = mesh->Vert(mesh->t[3 * tIdx]);
        Vec3f v1 = mesh->Vert(mesh->t[3 * tIdx + 1]);
        Vec3f v2 = mesh->Vert(mesh->t[3 * tIdx + 2]);
        float tt;
        if (RayTriangleIntersect(ro, rd, v0, v1, v2, bestT, tt)) {
          bestT = tt;
          hit = true;
        }
      }
    }
    if (tMaxX < tMaxY && tMaxX < tMaxZ) {
      if (tMaxX > bestT) {
        break;
      }
      cx += stepX;
      tMaxX += tDeltaX;
    } else if (tMaxY < tMaxZ) {
      if (tMaxY > bestT) {
        break;
      }
      cy += stepY;
      tMaxY += tDeltaY;
    } else {
      if (tMaxZ > bestT) {
        break;
      }
      cz += stepZ;
      tMaxZ += tDeltaZ;
    }
  }
  return hit;
}
