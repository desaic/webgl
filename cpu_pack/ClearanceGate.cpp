#include "ClearanceGate.h"

#include "GridUtils.h"
#include "Profiler.h"
#include "FloodOutside.h"
#include "MeshOps.h"
#include "cpu_voxelizer.h"

#include <algorithm>
#include <cmath>
#include <execution>
#include <numeric>

float InscribedRadius(const MeshInfo &item, float dx) {
  Box3f bbox = ComputeBBox(item.mesh.v);  // item.box can be stale after the inertia shift
  bbox.vmin = AlignOriginToGrid(bbox.vmin, dx);  // as PackingScene::Put does
  VoxConf conf;
  conf.origin = ToArray(AlignOriginToGrid(bbox.vmin, dx));
  conf.unit = {dx, dx, dx};
  conf.gridSize = ComputeGridSize(bbox, dx, 1);
  Array3D8u vox;
  vox.Allocate(conf.gridSize, 0);
  // voxelize the transformed copy, as PackingScene::Put does.
  VoxelizeMesh(MakeTransformedMesh(item.mesh, RigidTransform()), vox, conf);
  FloodOutside8u(vox, 1, 2);
  // outside voxels are the sources; interior distance to them is the clearance.
  Array3D8u outside;
  outside.Allocate(conf.gridSize, 0);
  for (size_t i = 0; i < vox.GetData().size(); i++) outside.GetData()[i] = vox.GetData()[i] == 0;
  const Array3D<float> d = DistanceToSource(outside);
  float best = 0.0f;
  for (float v : d.GetData()) best = std::max(best, v);
  return best * dx;
}

void ClearanceGate::Rebuild(const MeshConvo &bg, float cellSize, const Vec3u &numCells) {
  PROFILE_SCOPE("subgrid.clearance_gate");
  const Vec3u fs = bg.GridSize();
  const unsigned f = factor;
  coarseDx = bg.dx * float(f);
  const Vec3u cs((fs[0] + f - 1) / f, (fs[1] + f - 1) / f, (fs[2] + f - 1) / f);

  // coarse voxel is a source (occupied) only if every fine voxel in it is occupied.
  Array3D8u occupied;
  occupied.Allocate(cs, 1);
  std::vector<unsigned> zs(cs[2]);
  std::iota(zs.begin(), zs.end(), 0u);
  std::for_each(std::execution::par, zs.begin(), zs.end(), [&](unsigned Z) {
    const unsigned z1 = std::min(fs[2], (Z + 1) * f);
    for (unsigned z = Z * f; z < z1; z++)
      for (unsigned y = 0; y < fs[1]; y++)
        for (unsigned x = 0; x < fs[0]; x++)
          if (bg.vox(x, y, z) == 0) occupied(x / f, y / f, Z) = 0;
  });
  const Array3D<float> dist = DistanceToSource(occupied);

  // per cell: the best clearance anywhere within `reach` of the cell box. a fruit placed in the
  // cell can extend up to its own size past the cell boundary.
  const unsigned total = numCells[0] * numCells[1] * numCells[2];
  cellMax.assign(total, 0.0f);
  std::vector<unsigned> cells(total);
  std::iota(cells.begin(), cells.end(), 0u);
  const float voxCm = coarseDx;
  std::for_each(std::execution::par, cells.begin(), cells.end(), [&](unsigned ci) {
    const unsigned cx = ci % numCells[0];
    const unsigned cy = (ci / numCells[0]) % numCells[1];
    const unsigned cz = ci / (numCells[0] * numCells[1]);
    auto range = [&](unsigned c, unsigned n, unsigned size, unsigned &lo, unsigned &hi) {
      const float a = std::max(0.0f, float(c) * cellSize - reach);
      const float b = float(c + 1) * cellSize + reach;
      lo = std::min(size, unsigned(a / voxCm));
      hi = std::min(size, unsigned(std::ceil(b / voxCm)));
    };
    unsigned x0, x1, y0, y1, z0, z1;
    range(cx, numCells[0], cs[0], x0, x1);
    range(cy, numCells[1], cs[1], y0, y1);
    range(cz, numCells[2], cs[2], z0, z1);
    float m = 0.0f;
    for (unsigned z = z0; z < z1; z++)
      for (unsigned y = y0; y < y1; y++)
        for (unsigned x = x0; x < x1; x++) m = std::max(m, dist(x, y, z));
    cellMax[ci] = m * voxCm;
  });
  version = bg.version;
}
