#include "PocketVolume.h"

#include "Profiler.h"

#include <algorithm>
#include <cmath>
#include <deque>

namespace {

const int kNbr[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                        {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};

inline bool InBounds(const Vec3u &size, int x, int y, int z) {
  return x >= 0 && y >= 0 && z >= 0 && x < int(size[0]) && y < int(size[1])
         && z < int(size[2]);
}

inline bool IsFree(const Array3D8u &classGrid, const Vec3u &size, int x, int y, int z) {
  return InBounds(size, x, y, z)
         && classGrid(unsigned(x), unsigned(y), unsigned(z)) == VOX_FREE;
}

// keeps peaks spread across the component (see
// PocketVolumeParams::peakSeparationVoxels): a candidate within sepVoxels
// (Chebyshev) of an already-kept peak only replaces it if deeper, never
// adds a second entry next to it. Once the list is full, only replaces its
// current shallowest entry, so the kept set converges to the sepVoxels-or-
// more-apart local maxima found so far, not just the single global one.
void TryAddPeak(std::vector<FreeComponent::Peak> &peaks, unsigned maxPeaks,
                int sepVoxels, int d, const Vec3i &voxel) {
  for (FreeComponent::Peak &p : peaks) {
    int cheb = std::max({std::abs(voxel[0] - p.voxel[0]), std::abs(voxel[1] - p.voxel[1]),
                        std::abs(voxel[2] - p.voxel[2])});
    if (cheb < sepVoxels) {
      if (d > p.dist) {
        p.dist = d;
        p.voxel = voxel;
      }
      return;
    }
  }
  if (peaks.size() < maxPeaks) {
    FreeComponent::Peak p;
    p.dist = d;
    p.voxel = voxel;
    peaks.push_back(p);
    return;
  }
  unsigned minIdx = 0;
  for (unsigned i = 1; i < peaks.size(); i++) {
    if (peaks[i].dist < peaks[minIdx].dist) {
      minIdx = i;
    }
  }
  if (d > peaks[minIdx].dist) {
    peaks[minIdx].dist = d;
    peaks[minIdx].voxel = voxel;
  }
}

// multi-source BFS from every free voxel that has a non-free 6-neighbor
// (grid edges count as non-free, matching SurfaceCoverage's ClassAt
// out-of-bounds convention): an unweighted, 6-connected approximation of
// "distance to nearest wall," not an exact Euclidean distance transform.
Array3D<int> BuildDistGrid(const Array3D8u &classGrid) {
  PROFILE_SCOPE("volume.dist");
  Vec3u size = classGrid.GetSize();
  Array3D<int> dist(size, -1);
  std::deque<Vec3i> queue;
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        if (classGrid(x, y, z) != VOX_FREE) {
          continue;
        }
        bool onWall = false;
        for (const auto &n : kNbr) {
          if (!IsFree(classGrid, size, int(x) + n[0], int(y) + n[1], int(z) + n[2])) {
            onWall = true;
            break;
          }
        }
        if (onWall) {
          dist(x, y, z) = 1;
          queue.push_back(Vec3i(int(x), int(y), int(z)));
        }
      }
    }
  }
  while (!queue.empty()) {
    Vec3i c = queue.front();
    queue.pop_front();
    int d = dist(unsigned(c[0]), unsigned(c[1]), unsigned(c[2]));
    for (const auto &n : kNbr) {
      int nx = c[0] + n[0], ny = c[1] + n[1], nz = c[2] + n[2];
      if (!IsFree(classGrid, size, nx, ny, nz)) {
        continue;
      }
      if (dist(unsigned(nx), unsigned(ny), unsigned(nz)) >= 0) {
        continue;
      }
      dist(unsigned(nx), unsigned(ny), unsigned(nz)) = d + 1;
      queue.push_back(Vec3i(nx, ny, nz));
    }
  }
  return dist;
}

} // namespace

PocketVolumeField BuildPocketVolumeField(const Array3D8u &classGrid,
                                         const Vec3f &gridOrigin,
                                         const PocketVolumeParams &params) {
  PROFILE_SCOPE("volume.build");
  PocketVolumeField field;
  field.params = params;
  field.gridOrigin = gridOrigin;
  Vec3u size = classGrid.GetSize();
  field.componentId.Allocate(size, -1);

  Array3D<int> dist = BuildDistGrid(classGrid);

  PROFILE_SCOPE("volume.components");
  std::vector<Vec3i> stack;
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        if (classGrid(x, y, z) != VOX_FREE || field.componentId(x, y, z) >= 0) {
          continue;
        }
        unsigned compId = unsigned(field.components.size());
        FreeComponent comp;
        int minDepthVoxels = int(params.minTargetDepth / params.dx);
        field.componentId(x, y, z) = int(compId);
        stack.push_back(Vec3i(int(x), int(y), int(z)));
        while (!stack.empty()) {
          Vec3i c = stack.back();
          stack.pop_back();
          comp.voxelCount++;
          int d = dist(unsigned(c[0]), unsigned(c[1]), unsigned(c[2]));
          if (d >= minDepthVoxels) {
            TryAddPeak(comp.peaks, params.maxPeaksPerComponent,
                      int(params.peakSeparationVoxels), d, c);
          }
          for (const auto &n : kNbr) {
            int nx = c[0] + n[0], ny = c[1] + n[1], nz = c[2] + n[2];
            if (!IsFree(classGrid, size, nx, ny, nz)) {
              continue;
            }
            if (field.componentId(unsigned(nx), unsigned(ny), unsigned(nz)) >= 0) {
              continue;
            }
            field.componentId(unsigned(nx), unsigned(ny), unsigned(nz)) = int(compId);
            stack.push_back(Vec3i(nx, ny, nz));
          }
        }
        std::sort(comp.peaks.begin(), comp.peaks.end(),
                 [](const FreeComponent::Peak &a, const FreeComponent::Peak &b) {
                   return a.dist > b.dist;
                 });
        for (FreeComponent::Peak &p : comp.peaks) {
          p.point = gridOrigin
                   + params.dx * (Vec3f(float(p.voxel[0]), float(p.voxel[1]), float(p.voxel[2]))
                                 + Vec3f(0.5f));
        }
        if (!comp.peaks.empty()) {
          comp.targetDist = comp.peaks.front().dist;
          comp.targetVoxel = comp.peaks.front().voxel;
          comp.targetPoint = comp.peaks.front().point;
        }
        field.components.push_back(comp);
      }
    }
  }
  return field;
}
