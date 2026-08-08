#include "PackingOps.h"
#include "Profiler.h"

#include <cmath>

// exhaustive direct-overlap search, see PackingOps.h for why this beats an
// FFT at pocket-search box sizes.
bool FindSpotLocal(MeshConvo &bg,
                   const VoxFootprint &footprint,
                   const Box3f &searchBox,
                   const SpotScore &score,
                   Vec3f &pos) {
  PROFILE_SCOPE("findspot_local.total");
  float dx = bg.dx;
  Vec3f containerOrigin = bg.box.vmin;
  Vec3u bgSize = bg.GridSize();
  Vec3u fSize = footprint.vox.GetSize();

  // candidate positions must land on the same absolute dx lattice
  // footprint.localOrigin and bg's own origin are aligned to
  // (AlignOriginToGrid, GridUtils.cpp), or the cached footprint silently
  // misaligns against bg.vox by up to half a voxel.
  int xMin = (int)std::floor((searchBox.vmin[0] - containerOrigin[0]) / dx);
  int yMin = (int)std::floor((searchBox.vmin[1] - containerOrigin[1]) / dx);
  int zMin = (int)std::floor((searchBox.vmin[2] - containerOrigin[2]) / dx);
  int xMax = (int)std::ceil((searchBox.vmax[0] - containerOrigin[0]) / dx);
  int yMax = (int)std::ceil((searchBox.vmax[1] - containerOrigin[1]) / dx);
  int zMax = (int)std::ceil((searchBox.vmax[2] - containerOrigin[2]) / dx);

  bool found = false;
  float bestScore = -1e30f;
  Vec3f bestPos;

  for (int zi = zMin; zi <= zMax; zi++) {
    for (int yi = yMin; yi <= yMax; yi++) {
      for (int xi = xMin; xi <= xMax; xi++) {
        Vec3f p = containerOrigin + dx * Vec3f(float(xi), float(yi), float(zi));
        Vec3f baseF = (1.0f / dx) * (p + footprint.localOrigin - containerOrigin);
        int baseX = (int)std::round(baseF[0]);
        int baseY = (int)std::round(baseF[1]);
        int baseZ = (int)std::round(baseF[2]);

        bool blocked = false;
        for (unsigned fz = 0; fz < fSize[2] && !blocked; fz++) {
          int gz = baseZ + int(fz);
          if (gz < 0 || gz >= int(bgSize[2])) {
            blocked = true;
            break;
          }
          for (unsigned fy = 0; fy < fSize[1] && !blocked; fy++) {
            int gy = baseY + int(fy);
            if (gy < 0 || gy >= int(bgSize[1])) {
              blocked = true;
              break;
            }
            for (unsigned fx = 0; fx < fSize[0]; fx++) {
              if (footprint.vox(fx, fy, fz) == 0) {
                continue;
              }
              int gx = baseX + int(fx);
              if (gx < 0 || gx >= int(bgSize[0]) || bg.vox(unsigned(gx), unsigned(gy), unsigned(gz)) != 0) {
                blocked = true;
                break;
              }
            }
          }
        }
        if (blocked || !score.scorer) {
          continue;
        }

        float s = score.scorer(p, p);
        if (s > bestScore) {
          bestScore = s;
          bestPos = p;
          found = true;
        }
      }
    }
  }

  if (found) {
    pos = bestPos;
  }
  return found;
}
