#include "VoxFootprint.h"

#include "BBox.h"
#include "GridUtils.h"
#include "MeshConvo.h"
#include "MeshOps.h"
#include "TrigMesh.h"
#include "cpu_voxelizer.h"

const VoxFootprint &GetOrBuildFootprint(VoxFootprintCache &cache, unsigned kindId,
                                        unsigned rotIdx, const TrigMesh &part,
                                        const Vec3f &rot, float dx) {
  uint64_t key = VoxFootprintKey(kindId, rotIdx);
  auto it = cache.find(key);
  if (it != cache.end()) {
    return it->second;
  }

  Matrix3f rotMat = RotationMatrixRad(rot[0], rot[1], rot[2]);
  TrigMesh rotated = part;
  TransformVerts(part.v, rotated.v, rotMat);

  Box3f bbox = ComputeBBox(rotated.v);
  Vec3f originAligned = AlignOriginToGrid(bbox.vmin, dx);
  // ComputeGridSize needs the aligned corner in vmin, matching Put
  // (PackingScene.cpp): the aligned corner can sit up to dx before the true
  // vmin, and the grid must be big enough to still cover vmax from there.
  bbox.vmin = originAligned;

  VoxConf conf;
  conf.origin = ToArray(originAligned);
  conf.unit = {dx, dx, dx};
  conf.gridSize = ComputeGridSize(bbox, dx, 1);

  VoxFootprint fp;
  fp.vox.Allocate(conf.gridSize, 0);
  cpu_voxelize_grid(conf, &rotated, fp.vox);
  fp.localOrigin = originAligned;

  auto result = cache.emplace(key, std::move(fp));
  return result.first->second;
}

size_t VoxFootprintCacheBytes(const VoxFootprintCache &cache) {
  size_t bytes = 0;
  for (const auto &kv : cache) {
    bytes += kv.second.MemoryBytes();
  }
  return bytes;
}
