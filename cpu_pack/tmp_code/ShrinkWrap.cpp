#include "ShrinkWrap.h"

#include <deque>
#include <functional>

#include "AdapUDF.h"
#include "FastSweep.h"
#include "Math/VecUtil.h"
#include "SDFMesh.h"
#include "cpu_voxelizer.h"
#include "Mesh/MeshUtils.h"

size_t LinearIdx(unsigned x, unsigned y, unsigned z, const Vec3u& size) {
  return x + y * size_t(size[0]) + z * size_t(size[0] * size[1]);
}

Vec3u GridIdx(size_t l, const Vec3u& size) {
  unsigned x = l % size[0];
  unsigned y = (l % (size[0] * size[1])) / size[0];
  unsigned z = l / (size[0] * size[1]);
  return Vec3u(x, y, z);
}

void FloodOutsideSeed(Vec3u seed, const Array3D<short>& dist, float distThresh, Array3D8u& label) {
  Vec3u size = dist.GetSize();
  size_t linearSeed = LinearIdx(seed[0], seed[1], seed[2], size);
  std::deque<size_t> q(1, linearSeed);
  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  while (!q.empty()) {
    size_t linearIdx = q.front();
    q.pop_front();
    Vec3u idx = GridIdx(linearIdx, size);
    label(idx[0], idx[1], idx[2]) = 1;
    for (unsigned ni = 0; ni < NUM_NBR; ni++) {
      int nx = int(idx[0] + nbrOffset[ni][0]);
      int ny = int(idx[1] + nbrOffset[ni][1]);
      int nz = int(idx[2] + nbrOffset[ni][2]);
      if (!InBound(nx, ny, nz, size)) {
        continue;
      }
      unsigned ux = unsigned(nx), uy = unsigned(ny), uz = unsigned(nz);
      uint8_t nbrLabel = label(ux, uy, uz);
      size_t nbrLinear = LinearIdx(ux, uy, uz, size);
      if (nbrLabel == 0) {
        uint16_t nbrDist = dist(ux, uy, uz);
        if (nbrDist >= distThresh) {
          label(ux, uy, uz) = 1;
          q.push_back(nbrLinear);
        }
      }
    }
  }
}

/// <param name="dist">distance grid. at least 1 voxel of padding is assumed.</param>
/// <param name="distThresh">stop flooding if distance is less than this</param>
/// <returns>voxel labels. 1 for outside.</returns>
Array3D8u FloodOutside(const Array3D<short>& dist, float distThresh) {
  Array3D8u label;
  Vec3u size = dist.GetSize();
  label.Allocate(size[0], size[1], size[2]);
  label.Fill(0);
  // process easy voxels first.

  // all 6 faces are labeled to be outside regardless of actual distance
  // assuming the voxel grid has padding around the mesh.
  // all threads hanging from the faces are also labeld as outside quickly.
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      if (y == 0 || y == size[1] - 1 || z == 0 || z == size[2] - 1) {
        for (unsigned x = 0; x < size[0]; x++) {
          label(x, y, z) = 1;
        }
        continue;
      }
      label(0, y, z) = 1;
      label(size[0] - 1, y, z) = 1;
      for (unsigned x = 1; x < size[0] - 1; x++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned x = size[0] - 1; x > 0; x--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  for (unsigned x = 0; x < size[0]; x++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned z = 1; z < size[2] - 1; z++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned z = size[2] - 1; z > 0; z--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned x = 0; x < size[0]; x++) {
      for (unsigned y = 1; y < size[1] - 1; y++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned y = size[1] - 1; y > 0; y--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{-1, 0, 0}, {0, -1, 0}, {0, 0, -1}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  for (unsigned z = 1; z < size[2] - 1; z++) {
    for (unsigned y = 1; y < size[1] - 1; y++) {
      for (unsigned x = 1; x < size[0] - 1; x++) {
        for (unsigned ni = 0; ni < NUM_NBR; ni++) {
          unsigned ux = unsigned(x + nbrOffset[ni][0]);
          unsigned uy = unsigned(y + nbrOffset[ni][1]);
          unsigned uz = unsigned(z + nbrOffset[ni][2]);

          if (!label(x, y, z)) {
            continue;
          }
          uint8_t nbrLabel = label(ux, uy, uz);
          if (nbrLabel == 0) {
            uint16_t nbrDist = dist(ux, uy, uz);
            if (nbrDist >= distThresh) {
              FloodOutsideSeed(Vec3u(x, y, z), dist, distThresh, label);
            }
          }
        }
      }
    }
  }
  return label;
}

TrigMesh ShrinkWrap(const TrigMesh& input, float holeRadius, float voxelSize) {
  TrigMesh outMesh;
  if (input.NumTriangles() == 0 || input.NT().size() != input.T().size()) {
    return outMesh;
  }
  std::shared_ptr<AdapUDF> udf = std::make_shared<AdapUDF>();
  udf->voxSize = voxelSize;
  udf->band = 4;
  SDFImpAdap dfImp(udf);
  ShrinkWrapDistanceField(input, holeRadius, udf.get());
  dfImp.MarchingCubes(udf->voxSize, &outMesh);
  return outMesh;
}

struct SetDistFieldFun : public VoxCallback {
  virtual void operator()(unsigned x, unsigned y, unsigned z, size_t trigIdx) {
    if (fun) {
      fun(x, y, z, trigIdx);
    }
  }
  std::function<void(unsigned, unsigned, unsigned, size_t)> fun;
};

void ApproxShellDistanceField(const TrigMesh& mesh, AdapUDF* udf, float maxDistCell) {
  float h = udf->voxSize;
  udf->AllocateGrid(mesh);
  voxconf conf;
  conf.unit = Vec3f(h, h, h);
  // voxel grid is offset by half a voxel in negative direction
  conf.origin = udf->origin - 0.5f * conf.unit;
  conf.gridSize = udf->dist.GetSize() + Vec3u(1, 1, 1);
  SetDistFieldFun setDist;
  udf->dist.Fill(AdapUDF::MAX_DIST);
  setDist.fun = [udf](unsigned x, unsigned y, unsigned z, size_t tIdx) {
    const Vec3u size = udf->dist.GetSize();
    if (x < size[0] && y < size[1] && z < size[2]) {
      udf->dist(x, y, z) = 0;
    }
  };
  cpu_voxelize_mesh(conf, &mesh, setDist);
  Array3D8u frozen;
  frozen = FloodOutside(udf->dist, h / 2 / udf->distUnit);
  // everything inside is frozen since we are not generating cage inside.
  for (size_t i = 0; i < frozen.GetData().size(); i++) {
    uint8_t f = frozen.GetData()[i];
    short d = udf->dist.GetData()[i];
    if ((!f) && d >= AdapDF::MAX_DIST) {
      udf->dist.GetData()[i] = 0;
    }
    frozen.GetData()[i] = 1 - f;
  }

  FastSweepParUnsigned(udf->dist, udf->voxSize, udf->distUnit, maxDistCell, frozen);
}

/// @brief helper that computes the distance field for shrinkwrap.
void ShrinkWrapDistanceField(const TrigMesh& input, float holeRadius, AdapUDF* udf) {
  float h = udf->voxSize;
  float distUnit = 2.5e-2;
  udf->distUnit = distUnit;
  // stop propagating distance values greater than this
  // measured in voxels.
  float maxDist = std::max(2.0f, holeRadius / h);
  ApproxShellDistanceField(input, udf, maxDist);
  Array3D8u frozen;
  // distance >= thresh is considered outside.
  float outsideThresh = (holeRadius - udf->voxSize) / udf->distUnit;
  frozen = FloodOutside(udf->dist, outsideThresh);
  Vec3u size = frozen.GetSize();
  // distance from outermost shell
  Array3D<short> borderDist;
  borderDist.Allocate(size[0], size[1], size[2]);
  for (size_t i = 0; i < frozen.GetData().size(); i++) {
    if (frozen.GetData()[i] > 0) {
      borderDist.GetData()[i] =
          std::max(short(0), short(holeRadius / distUnit - udf->dist.GetData()[i]));
      frozen.GetData()[i] = 1;
    } else {
      borderDist.GetData()[i] = AdapDF::MAX_DIST;
    }
  }

  FastSweepParUnsigned(borderDist, udf->voxSize, distUnit, maxDist + 2, frozen);
  
  for (size_t i = 0; i < borderDist.GetData().size(); i++) {
    //set outside voxels to infinity to make sure always close the interior mesh
    if (frozen.GetData()[i]) {
      udf->dist.GetData()[i] = AdapDF::MAX_DIST; 
      continue;
    }
    short d = borderDist.GetData()[i];
    if (d >= AdapDF::MAX_DIST) {
      d = -2 * h / distUnit;
    } else {
      d = short((holeRadius - 0.5f * h) / distUnit - d);
    }

    udf->dist.GetData()[i] = std::min(udf->dist.GetData()[i], d);
  }
}