#include "ContainerShell.h"

#include "FloodOutside.h"
#include "GridUtils.h"
#include "Log.h"
#include "MarchingCubes.h"
#include "MeshInfo.h"
#include "MeshOps.h"
#include "MeshOptimization.h"
#include "Stopwatch.h"
#include "cpu_voxelizer.h"
#include "meshutil.h"

#include <algorithm>
#include <cmath>
#include <vector>

static double ShellArea(const TrigMesh &m) {
  double area = 0.0;
  for (size_t i = 0; i + 2 < m.t.size(); i += 3) {
    const float *a = &m.v[3 * m.t[i]];
    const float *b = &m.v[3 * m.t[i + 1]];
    const float *c = &m.v[3 * m.t[i + 2]];
    const double ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2];
    const double wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
    const double cx = uy * wz - uz * wy;
    const double cy = uz * wx - ux * wz;
    const double cz = ux * wy - uy * wx;
    area += 0.5 * std::sqrt(cx * cx + cy * cy + cz * cz);
  }
  return area;
}

// 1D squared distance transform of a sampled function (Felzenszwalb and
// Huttenlocher). f and d have n entries; v and z are scratch of n and n + 1.
static void Edt1D(const float *f, float *d, int n, int *v, float *z) {
  int k = 0;
  v[0] = 0;
  z[0] = -1e30f;
  z[1] = 1e30f;
  for (int q = 1; q < n; q++) {
    float s;
    while (true) {
      const int p = v[k];
      s = ((f[q] + float(q) * q) - (f[p] + float(p) * p)) / (2.0f * float(q - p));
      if (s <= z[k] && k > 0) {
        k--;
      } else {
        break;
      }
    }
    k++;
    v[k] = q;
    z[k] = s;
    z[k + 1] = 1e30f;
  }
  k = 0;
  for (int q = 0; q < n; q++) {
    while (z[k + 1] < float(q)) {
      k++;
    }
    const float dq = float(q - v[k]);
    d[q] = dq * dq + f[v[k]];
  }
}

// distance in voxels from every voxel to the nearest voxel with source != 0.
static Array3D<float> DistanceToSource(const Array3D8u &source) {
  const Vec3u size = source.GetSize();
  const float INF = 1e12f;
  Array3D<float> g;
  g.Allocate(size, INF);
  for (unsigned z = 0; z < size[2]; z++)
    for (unsigned y = 0; y < size[1]; y++)
      for (unsigned x = 0; x < size[0]; x++)
        if (source(x, y, z)) g(x, y, z) = 0.0f;

  const unsigned maxN = std::max(size[0], std::max(size[1], size[2]));
  std::vector<float> f(maxN), d(maxN), z(maxN + 1);
  std::vector<int> v(maxN);
  for (unsigned zi = 0; zi < size[2]; zi++) {
    for (unsigned yi = 0; yi < size[1]; yi++) {
      for (unsigned xi = 0; xi < size[0]; xi++) f[xi] = g(xi, yi, zi);
      Edt1D(f.data(), d.data(), int(size[0]), v.data(), z.data());
      for (unsigned xi = 0; xi < size[0]; xi++) g(xi, yi, zi) = d[xi];
    }
  }
  for (unsigned zi = 0; zi < size[2]; zi++) {
    for (unsigned xi = 0; xi < size[0]; xi++) {
      for (unsigned yi = 0; yi < size[1]; yi++) f[yi] = g(xi, yi, zi);
      Edt1D(f.data(), d.data(), int(size[1]), v.data(), z.data());
      for (unsigned yi = 0; yi < size[1]; yi++) g(xi, yi, zi) = d[yi];
    }
  }
  for (unsigned yi = 0; yi < size[1]; yi++) {
    for (unsigned xi = 0; xi < size[0]; xi++) {
      for (unsigned zi = 0; zi < size[2]; zi++) f[zi] = g(xi, yi, zi);
      Edt1D(f.data(), d.data(), int(size[2]), v.data(), z.data());
      for (unsigned zi = 0; zi < size[2]; zi++) g(xi, yi, zi) = std::sqrt(d[zi]);
    }
  }
  return g;
}

// Inside and outside come only from voxelization and a flood fill from the
// grid border; the signed distance field of the triangles is not used at all.
// The solid is the flooded interior plus the surface voxels that touch it, so
// surface voxels with no interior behind them (dangling slivers, fins) are
// dropped and cannot grow into offset tubes.
static TrigMesh ComputeOffsetSurfaceFromVoxels(const TrigMesh &mesh, float dx, float outset) {
  Box3f box = ComputeBBox(mesh.v);
  const float margin = 3.0f * dx + std::max(outset, 0.0f);
  box.vmin = AlignOriginToGrid(box.vmin - Vec3f(margin), dx);
  const Vec3f vmax = box.vmax + Vec3f(margin);
  VoxConf conf;
  conf.origin = Vec3f(box.vmin);
  conf.unit = Vec3f(dx);
  for (unsigned a = 0; a < 3; a++) {
    conf.gridSize[a] = unsigned(std::ceil((vmax[a] - box.vmin[a]) / dx)) + 1;
  }
  Array3D8u vox;
  VoxelizeMesh(mesh, vox, conf);
  // exterior 0, surface 1, interior 2.
  FloodOutside8u(vox, 1, 2);

  const Vec3u size = vox.GetSize();
  Array3D8u solid;
  solid.Allocate(size, 0);
  size_t numInterior = 0, numSurface = 0, numSolidSurface = 0;
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        const uint8_t val = vox(x, y, z);
        if (val == 2) {
          numInterior++;
          continue;
        }
        if (val != 1) continue;
        numSurface++;
        bool touchesInterior = false;
        for (int dz = -1; dz <= 1 && !touchesInterior; dz++) {
          for (int dy = -1; dy <= 1 && !touchesInterior; dy++) {
            for (int dxi = -1; dxi <= 1; dxi++) {
              const int nx = int(x) + dxi, ny = int(y) + dy, nz = int(z) + dz;
              if (nx < 0 || ny < 0 || nz < 0 || nx >= int(size[0]) || ny >= int(size[1]) ||
                  nz >= int(size[2]))
                continue;
              if (vox(unsigned(nx), unsigned(ny), unsigned(nz)) == 2) {
                touchesInterior = true;
                break;
              }
            }
          }
        }
        if (touchesInterior) {
          solid(x, y, z) = 1;
          numSolidSurface++;
        }
      }
    }
  }
  LOGI("container shell voxels: grid " << size[0] << "x" << size[1] << "x" << size[2] << ", interior "
                                       << numInterior << ", surface " << numSurface << " of which "
                                       << numSolidSurface << " touch the interior\n");

  // signed distance (voxels) to the solid-surface voxel centers: 0 on them,
  // negative in the interior, positive elsewhere.
  Array3D<float> field = DistanceToSource(solid);
  for (unsigned z = 0; z < size[2]; z++)
    for (unsigned y = 0; y < size[1]; y++)
      for (unsigned x = 0; x < size[0]; x++)
        if (vox(x, y, z) == 2) field(x, y, z) = -field(x, y, z);

  TrigMesh surf;
  MarchingCubes(field, outset / dx, &surf, 1.0f);
  // marching cubes samples sit at voxel centers.
  for (size_t i = 0; i + 2 < surf.v.size(); i += 3) {
    for (unsigned a = 0; a < 3; a++) {
      surf.v[i + a] = (surf.v[i + a] + 0.5f) * dx + conf.origin[a];
    }
  }
  MergeCloseVertices(surf);
  return surf;
}

TrigMesh ComputeContainerShell(PackingScene &scene, const PackingConfig &cfg) {
  Utils::Stopwatch clock;
  clock.Start();
  TrigMesh surf;
  if (cfg.shellFromVoxels) {
    surf = ComputeOffsetSurfaceFromVoxels(scene.container.mesh, cfg.shellVoxel, cfg.shellOutset);
  } else {
    // ComputeInnerSurface: positive offset moves inward, so a negative one
    // gives the outset surface.
    surf = ComputeInnerSurface(scene.container.mesh, cfg.shellVoxel, -cfg.shellOutset);
  }
  const size_t trigsBefore = surf.t.size() / 3;

  TrigMesh shell;
  MeshOptimization::ComputeSimplifiedMesh(surf.v, surf.t, cfg.shellError, cfg.shellFraction,
                                          shell.v, shell.t);
  LOGI("container shell: " << trigsBefore << " -> " << shell.t.size() / 3 << " triangles, area "
                           << ShellArea(surf) << " -> " << ShellArea(shell) << " cm2, "
                           << clock.ElapsedMS() / 1000.0 << " s\n");
  return shell;
}

TrigMesh ComputeContainerPatches(PackingScene &scene, const PackingConfig &cfg,
                                 std::vector<ShellPatch> &patches) {
  TrigMesh shell = ComputeContainerShell(scene, cfg);
  patches = BuildShellPatches(shell, cfg.patchRadius, cfg.patchAngle);
  size_t refs = 0;
  for (const ShellPatch &p : patches) refs += p.trigs.size();
  LOGI("container patches: " << patches.size() << " patches over " << shell.t.size() / 3
                             << " triangles, " << refs << " triangle refs\n");
  return shell;
}

void ExportContainerShell(PackingScene &scene, const PackingConfig &cfg) {
  if (cfg.shellVoxel <= 0.0f) return;
  std::vector<ShellPatch> patches;
  TrigMesh shell = ComputeContainerPatches(scene, cfg, patches);
  if (shell.t.empty()) {
    LOGI("container shell: empty result\n");
    return;
  }
  shell.SaveObj(scene.outputFolder + "/container_shell.obj");
  SavePatchesObj(scene.outputFolder + "/container_patches.obj", shell, patches);
}
