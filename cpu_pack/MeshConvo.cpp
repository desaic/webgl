#include "MeshConvo.h"
#include "cpu_voxelizer.h"
#include "pocketfft_3df.h"
#include "FloodOutside.h"
#include "GridUtils.h"

#include <thread>
#include <cstring>

unsigned PadSize(unsigned s, unsigned alignment) {
  if ((s > 0) && (s % alignment == 0)) {
    return s;
  }
  return s + alignment - s % alignment;
}

std::array<unsigned, 3> PadSizes(Vec3u &s, unsigned alignment) {
  return {PadSize(s[0], alignment), PadSize(s[1], alignment), PadSize(s[2], alignment)};
}

std::array<unsigned, 3> ComputeGridSize(const Box3f &box, float dx, unsigned alignment) 
{
  Vec3f boxSize = box.vmax - box.vmin;
  std::array<unsigned,3> size;
  for (unsigned d = 0; d < 3; d++) {
    size[d] = unsigned(boxSize[d] / dx) + 2;
    size[d] = PadSize(size[d], alignment);
  }
  return size;
}

static void VoxelizeMesh(const TrigMesh &m, Array3D8u &grid, VoxConf conf) {
  grid.Allocate(conf.gridSize, 0);
  cpu_voxelize_grid(conf, &m, grid);
}

void MeshConvo::Voxelize(float voxelSize) {
  if (mesh == nullptr) {
    return;
  }
  dx = voxelSize;
  meshBox = ComputeBBox(mesh->v);
  box = meshBox;
  box.vmin = box.vmin - Vec3f(dx);
  box.vmin = AlignOriginToGrid(box.vmin, dx);
  VoxConf conf;
  conf.origin = ToArray(box.vmin);
  conf.unit = {dx, dx, dx};

  const unsigned FFT_ALIGNMENT = 8;
  conf.gridSize = ComputeGridSize(box, dx, FFT_ALIGNMENT);
  VoxelizeMesh(*mesh, vox, conf);
  FloodOutside8u(vox, 1, 2);
}

Array3D8u MeshConvo::PadVox(Vec3u newSize) const {
  Vec3u size = GridSize();
  // new size much >= old size in all dimensions
  if (newSize[0] < size[0] || newSize[1] < size[1] || newSize[2] < size[2]) {
    return Array3D8u();
  }
  Array3D8u padded(newSize, 0);
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      const uint8_t *srcRow = (const uint8_t *)(&vox(0, y, z));
      uint8_t *dstRow = &padded(0, y, z);
      size_t len = size[0];
      std::memcpy(dstRow, srcRow, len);
    }
  }
  return padded;
}
// since we only care about collision, all images are integer valued.
// we should use NTT instead and always pad to powers of 2 up to 1024.
// should limit to binary values to prevent overflow of coefficients to > 2^32
// can do check for min(pixel_count1, pixel_count2)
// if too many pixels in both , the result is invalid.
void MeshConvo::FFT(Vec3u paddedSize) {
  Array3D8u padded = PadVox(paddedSize);

  Vec3u complexSize = paddedSize;
  complexSize[2] = paddedSize[2] / 2 + 1;
  fft.Allocate(complexSize, 0);
  size_t nthreads = std::thread::hardware_concurrency();
  pocketfft::shape_t shape_in(3);
  shape_in[0] = paddedSize[0];
  shape_in[1] = paddedSize[1];
  shape_in[2] = paddedSize[2];
  pocketfft::r2c_3d8u(shape_in, padded.DataPtr(), fft.DataPtr(), nthreads);
}

void Union(MeshConvo &bg, Vec3i offset, const Array3D8u &fg) {
  Vec3u size = fg.GetSize();
  Vec3u bgSize = bg.GridSize();
  for (unsigned z = 0; z < size[2]; z++) {
    int dstz = int(z) + offset[2];
    if (dstz < 0 || dstz >= int(bgSize[2])) {
      continue;
    }
    for (unsigned y = 0; y < size[1]; y++) {
      int dsty = int(y) + offset[1];
      if (dsty < 0 || dsty >= int(bgSize[1])) {
        continue;
      }
      for (unsigned x = 0; x < size[0]; x++) {
        int dstx = int(x) + offset[0];
        if (dstx < 0 || dstx >= int(bgSize[0])) {
          continue;
        }
        if (fg(x, y, z) > 0) {
          bg.vox(dstx, dsty, dstz) = 1;
        }
      }
    }
  }
}

void UnionReversed(MeshConvo &bg, Vec3i offset, const MeshConvo &fg) {
  Vec3u size = fg.GridSize();
  Vec3u bgSize = bg.GridSize();
  for (unsigned z = 0; z < size[2]; z++) {
    int dstz = int(z) + offset[2];
    if (dstz < 0 || dstz >= int(bgSize[2])) {
      continue;
    }
    for (unsigned y = 0; y < size[1]; y++) {
      int dsty = int(y) + offset[1];
      if (dsty < 0 || dsty >= int(bgSize[1])) {
        continue;
      }
      for (unsigned x = 0; x < size[0]; x++) {
        int dstx = int(x) + offset[0];
        if (dstx < 0 || dstx >= int(bgSize[0])) {
          continue;
        }
        if (fg.vox(size[0] - x - 1, size[1] - y - 1, size[2]- z- 1) > 0) {
          bg.vox(dstx, dsty, dstz) = 1;
        }
      }
    }
  }
}
