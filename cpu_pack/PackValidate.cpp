#include "PackValidate.h"

#include "FloodOutside.h"
#include "GridUtils.h"
#include "MeshOps.h"
#include "PointSample.h"
#include "cpu_voxelizer.h"

#include <cmath>
#include <cstdlib>
#include <sstream>

std::string ValidationResult::toString() const {
  std::ostringstream oss;
  oss << (Ok() ? "OK" : "FAIL");
  oss << " samples " << numSamples;
  oss << " outside " << outsideCount;
  oss << " overlap " << overlapCount;
  if (offGridCount > 0) {
    oss << " offGrid " << offGridCount;
  }
  oss << " (deeper than " << allowedDepth << " cm)";
  oss << " worstOutside " << maxOutsideDepth;
  oss << " worstOverlap " << maxOverlapDepth;
  return oss.str();
}

void PackValidator::Init(const PackingScene &scene, const PackingConfig &cfg) {
  dx = cfg.dx;

  // voxelize the container the same way PrepareBackground does, but into a
  // private grid so this does not depend on how many items have been Put.
  TrigMesh containerCopy = scene.container.mesh;
  Box3f box = ComputeBBox(containerCopy.v);
  box.vmin = box.vmin - Vec3f(dx);
  box.vmin = AlignOriginToGrid(box.vmin, dx);

  VoxConf conf;
  conf.origin = ToArray(box.vmin);
  conf.unit = {dx, dx, dx};
  const unsigned FFT_ALIGNMENT = 8;
  conf.gridSize = ComputeGridSize(box, dx, FFT_ALIGNMENT);

  containerVox.Allocate(conf.gridSize, 0);
  cpu_voxelize_grid(conf, &containerCopy, containerVox);
  // exterior 0, shell 1, interior 2.
  FloodOutside8u(containerVox, 1, 2);
  // shell and exterior become 1 (occupied), interior becomes 0 (free).
  InvertContainer(containerVox, 1);

  origin = box.vmin;
  gridSize = conf.gridSize;
  fruitVox.Allocate(gridSize, 0);
  initialized = true;
}

bool PackValidator::WorldToGrid(const Vec3f &p, Vec3u &idx) const {
  Vec3f rel = (p - origin) * (1.0f / dx);
  if (rel[0] < 0 || rel[1] < 0 || rel[2] < 0) {
    return false;
  }
  Vec3u u(static_cast<unsigned>(rel[0]), static_cast<unsigned>(rel[1]),
          static_cast<unsigned>(rel[2]));
  if (u[0] >= gridSize[0] || u[1] >= gridSize[1] || u[2] >= gridSize[2]) {
    return false;
  }
  idx = u;
  return true;
}

unsigned PackValidator::AllowedDepthVoxels() const {
  if (dx <= 0.0f) {
    return 0;
  }
  float v = allowedDepth / dx;
  if (v <= 0.0f) {
    return 0;
  }
  return unsigned(std::floor(v + 1e-4f));
}

unsigned PackValidator::DepthVoxelsAt(const Array3D8u &vox,
                                      const Vec3u &idx) const {
  if (vox(idx[0], idx[1], idx[2]) == 0) {
    return 0;
  }
  // grow a cube around idx until one of its faces touches a free voxel.
  // the last fully occupied radius is the depth, so a sample sitting right
  // on a surface reports ~dx while one buried deep reports much more.
  for (unsigned r = 1; r <= maxDepthVoxels; r++) {
    int ri = int(r);
    for (int dz = -ri; dz <= ri; dz++) {
      for (int dy = -ri; dy <= ri; dy++) {
        for (int dx_i = -ri; dx_i <= ri; dx_i++) {
          // only the shell of the cube is new at this radius.
          if (std::abs(dx_i) != ri && std::abs(dy) != ri
              && std::abs(dz) != ri) {
            continue;
          }
          int gx = int(idx[0]) + dx_i;
          int gy = int(idx[1]) + dy;
          int gz = int(idx[2]) + dz;
          if (gx < 0 || gy < 0 || gz < 0 || gx >= int(gridSize[0])
              || gy >= int(gridSize[1]) || gz >= int(gridSize[2])) {
            // treat the grid edge as free so we never overreport.
            return r;
          }
          if (vox(unsigned(gx), unsigned(gy), unsigned(gz)) == 0) {
            return r;
          }
        }
      }
    }
  }
  return maxDepthVoxels + 1;
}

ValidationResult PackValidator::ValidatePlacement(const PackingScene &scene,
                                                  unsigned itemIdx,
                                                  const RigidTransform &tran) {
  ValidationResult res;
  if (!initialized || itemIdx >= scene.items.size()) {
    return res;
  }
  const MeshInfo &item = scene.items[itemIdx];

  // reuse the contact samples if Nudge already built them, else sample now.
  std::vector<SamplePoint> samples = item.samples;
  if (samples.empty()) {
    TrigMesh meshCopy = item.mesh;
    if (meshCopy.nv.empty()) {
      meshCopy.ComputeTrigNormals();
      meshCopy.ComputeVertNormals();
    }
    SamplePoints(meshCopy, sampleSpacing, samples);
  }
  res.numSamples = unsigned(samples.size());
  unsigned allowVox = AllowedDepthVoxels();
  // report the threshold actually applied, not the requested one.
  res.allowedDepth = dx * float(allowVox);

  for (size_t i = 0; i < samples.size(); i++) {
    Vec3f world = tran.rotation * samples[i].x + tran.position;
    Vec3u idx;
    if (!WorldToGrid(world, idx)) {
      res.offGridCount++;
      continue;
    }
    unsigned outDepth = DepthVoxelsAt(containerVox, idx);
    if (dx * float(outDepth) > res.maxOutsideDepth) {
      res.maxOutsideDepth = dx * float(outDepth);
    }
    if (outDepth > allowVox) {
      res.outsideCount++;
    }

    unsigned ovDepth = DepthVoxelsAt(fruitVox, idx);
    if (dx * float(ovDepth) > res.maxOverlapDepth) {
      res.maxOverlapDepth = dx * float(ovDepth);
    }
    if (ovDepth > allowVox) {
      res.overlapCount++;
    }
  }
  return res;
}

void PackValidator::AddPlaced(const PackingScene &scene, unsigned itemIdx,
                              const RigidTransform &tran) {
  if (!initialized || itemIdx >= scene.items.size()) {
    return;
  }
  TrigMesh inst = MakeTransformedMesh(scene.items[itemIdx].mesh, tran);
  Box3f bbox = ComputeBBox(inst.v);
  bbox.vmin = AlignOriginToGrid(bbox.vmin, dx);

  VoxConf conf;
  conf.origin = ToArray(bbox.vmin);
  conf.unit = {dx, dx, dx};
  conf.gridSize = ComputeGridSize(bbox, dx, 1);

  Array3D8u vox;
  vox.Allocate(conf.gridSize, 0);
  cpu_voxelize_grid(conf, &inst, vox);
  // shell 1, interior 2, exterior 0. keep shell and interior as occupied.
  FloodOutside8u(vox, 1, 2);

  Vec3i offset(int(std::round((bbox.vmin[0] - origin[0]) / dx)),
               int(std::round((bbox.vmin[1] - origin[1]) / dx)),
               int(std::round((bbox.vmin[2] - origin[2]) / dx)));

  Vec3u size = vox.GetSize();
  for (unsigned z = 0; z < size[2]; z++) {
    int dz = int(z) + offset[2];
    if (dz < 0 || dz >= int(gridSize[2])) continue;
    for (unsigned y = 0; y < size[1]; y++) {
      int dy = int(y) + offset[1];
      if (dy < 0 || dy >= int(gridSize[1])) continue;
      for (unsigned x = 0; x < size[0]; x++) {
        int dxi = int(x) + offset[0];
        if (dxi < 0 || dxi >= int(gridSize[0])) continue;
        if (vox(x, y, z) > 0) {
          fruitVox(unsigned(dxi), unsigned(dy), unsigned(dz)) = 1;
        }
      }
    }
  }
}

void PackValidator::AddAllPlaced(const PackingScene &scene) {
  for (size_t i = 0; i < scene.instances.size(); i++) {
    AddPlaced(scene, scene.instances[i].itemId, scene.instances[i].tran);
  }
}

size_t PackValidator::MemoryBytes() const {
  return containerVox.GetData().size() + fruitVox.GetData().size();
}
