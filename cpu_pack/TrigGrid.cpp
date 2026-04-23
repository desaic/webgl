#include "TrigGrid.h"
#include "TrigMesh.h"
#include "cpu_voxelizer.h"

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

float TrigGrid::NearestTriangle(const Vec3f &point, float maxDist) const {
  // Convert point to grid coordinates
  Vec3f gridCoord = (point - origin) * (1.0f / voxelSize);
  Vec3i gridIdx = Vec3i(int(gridCoord[0]), int(gridCoord[1]), int(gridCoord[2]));

  float minDist = maxDist;
  Vec3u gridSize = grid.GetSize();
  // Search 3x3x3 neighborhood
  for (int dz = -1; dz <= 1; dz++) {
    int gz = gridIdx[2] + dz;
    if (gz < 0 || gz >= int(gridSize[2]))
      continue;

    for (int dy = -1; dy <= 1; dy++) {
      int gy = gridIdx[1] + dy;
      if (gy < 0 || gy >= int(gridSize[1]))
        continue;

      for (int dx_iter = -1; dx_iter <= 1; dx_iter++) {
        int gx = gridIdx[0] + dx_iter;
        if (gx < 0 || gx >= int(gridSize[0]))
          continue;

        uint32_t listIdx = grid(gx, gy, gz);
        if (listIdx == 0)
          continue; // Empty voxel

        const std::vector<unsigned> &trigList = tLists[listIdx];

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
          }
        }
      }
    }
  }

  return minDist;
}
