#include "PackShrinkWrap.h"

#include "AdapSDF.h"
#include "FastSweep.h"
#include "Log.h"
#include "MarchingCubes.h"
#include "MeshOps.h"
#include "PackingScene.h"
#include "cpu_voxelizer.h"
#include "FloodOutside.h"
#include "BBox.h"
#include "meshutil.h"

#include <algorithm>
#include <deque>

namespace {

size_t LinearIdx(unsigned x, unsigned y, unsigned z, const Vec3u &size) {
  return x + y * size_t(size[0]) + z * size_t(size[0] * size[1]);
}

Vec3u GridIdx(size_t l, const Vec3u &size) {
  unsigned x = l % size[0];
  unsigned y = (l % (size[0] * size[1])) / size[0];
  unsigned z = l / (size[0] * size[1]);
  return Vec3u(x, y, z);
}

bool InBound(int x, int y, int z, const Vec3u &size) {
  return x >= 0 && y >= 0 && z >= 0 && (unsigned)x < size[0] &&
         (unsigned)y < size[1] && (unsigned)z < size[2];
}

void FloodOutsideSeed(Vec3u seed, const Array3D<short> &dist, float distThresh,
                      Array3D8u &label) {
  Vec3u size = dist.GetSize();
  size_t linearSeed = LinearIdx(seed[0], seed[1], seed[2], size);
  std::deque<size_t> q(1, linearSeed);
  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
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
        short nbrDist = dist(ux, uy, uz);
        if (nbrDist >= distThresh) {
          label(ux, uy, uz) = 1;
          q.push_back(nbrLinear);
        }
      }
    }
  }
}

Array3D8u FloodOutsideShrink(const Array3D<short> &dist, float distThresh) {
  Array3D8u label;
  Vec3u size = dist.GetSize();
  label.Allocate(size[0], size[1], size[2]);
  label.Fill(0);

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
  const int nbrOffset[6][3] = {{-1, 0, 0}, {0, -1, 0}, {0, 0, -1},
                                {1, 0, 0},  {0, 1, 0},  {0, 0, 1}};
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
            short nbrDist = dist(ux, uy, uz);
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

void ComputeShrinkWrapDistField(const Array3D8u &occupancy, float voxelSize,
                                 float shrinkRadius, Array3D<short> &dist,
                                 float &distUnit) {
  // distance field is defined on voxel vertices.
  Vec3u size = occupancy.GetSize() + Vec3u(1, 1, 1);
  dist.Allocate(size[0], size[1], size[2]);
  distUnit = 0.01f;

  const short MAX_DIST = 32700;
  dist.Fill(MAX_DIST);
  // coarse approximation. could use adapudf instead of voxelize meshes.
  // for now set all 8 vertices of an occupied voxel to 0
  for (unsigned z = 0; z < size[2] - 1;z++){
    for (unsigned y = 0; y < size[1] - 1;y++){
      for (unsigned x = 0; x < size[0] - 1;x++){
        if(occupancy(x,y,z) > 0){
          dist(x, y, z) = 0;
          dist(x + 1, y, z) = 0;
          dist(x, y + 1, z) = 0;
          dist(x + 1, y + 1, z) = 0;
          dist(x, y, z + 1) = 0;
          dist(x + 1, y, z + 1) = 0;
          dist(x, y + 1, z + 1) = 0;
          dist(x + 1, y + 1, z + 1) = 0;
        }
      }
    }
  }

  Array3D8u vox(size, 0);
  for (unsigned z = 0; z < size[2] ; z++) {
    for (unsigned y = 0; y < size[1] ; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        if(dist(x,y,z)==0){
          vox(x, y, z) = 1;
        }
      }
    }
  }
  Vec3f voxRes(voxelSize, voxelSize, voxelSize);
  std::string voxFile = "F:/meshes/fruit_hand/out_melone_test/dist_voxels.obj";
  std::cout << "saving vox file " << voxFile << "\n";
  SaveVolAsObjMesh(voxFile, vox, voxRes, Vec3f(0), 1);

  float maxDist = std::max(2.0f, shrinkRadius / voxelSize);
  Array3D8u frozen;
  frozen.Allocate(size[0], size[1], size[2]);
  frozen.Fill(0);

  FastSweepParUnsigned(dist, voxelSize, distUnit, maxDist, frozen);

  float outsideThresh = (shrinkRadius - voxelSize) / distUnit;
  frozen = FloodOutsideShrink(dist, outsideThresh);

  Array3D<short> borderDist;
  borderDist.Allocate(size[0], size[1], size[2]);
  for (size_t i = 0; i < frozen.GetData().size(); i++) {
    if (frozen.GetData()[i] > 0) {
      borderDist.GetData()[i] =
          std::max(short(0), short(shrinkRadius / distUnit - dist.GetData()[i]));
      frozen.GetData()[i] = 1;
    } else {
      borderDist.GetData()[i] = MAX_DIST;
    }
  }

  FastSweepParUnsigned(borderDist, voxelSize, distUnit, maxDist + 2, frozen);

  for (size_t i = 0; i < borderDist.GetData().size(); i++) {
    if (frozen.GetData()[i]) {
      dist.GetData()[i] = MAX_DIST;
      continue;
    }
    short d = borderDist.GetData()[i];
    if (d >= MAX_DIST) {
      d = -2 * voxelSize / distUnit;
    } else {
      d = short((shrinkRadius - 0.5f * voxelSize) / distUnit - d);
    }
    dist.GetData()[i] = std::min(dist.GetData()[i], d);
  }
}

Vec3i RoundVec(const Vec3f &fvec) {
  return Vec3i(std::round(fvec[0]), std::round(fvec[1]), std::round(fvec[2]));
}

}  // namespace

Array3D8u VoxelizeItems(PackingScene &scene, float voxelSize, Vec3f &outOrigin) {
  Box3f totalBox;
  totalBox.vmin = Vec3f(1e10f, 1e10f, 1e10f);
  totalBox.vmax = Vec3f(-1e10f, -1e10f, -1e10f);

  for (const auto &inst : scene.instances) {
    unsigned itemIdx = inst.itemId;
    const RigidTransform &tran = inst.tran;
    TrigMesh transformedMesh = MakeTransformedMesh(scene.items[itemIdx].mesh, tran);
    Box3f bbox = ComputeBBox(transformedMesh.v);
    totalBox.vmin = Vec3f(std::min(totalBox.vmin[0], bbox.vmin[0]),
                          std::min(totalBox.vmin[1], bbox.vmin[1]),
                          std::min(totalBox.vmin[2], bbox.vmin[2]));
    totalBox.vmax = Vec3f(std::max(totalBox.vmax[0], bbox.vmax[0]),
                          std::max(totalBox.vmax[1], bbox.vmax[1]),
                          std::max(totalBox.vmax[2], bbox.vmax[2]));
  }

  totalBox.vmin = AlignOriginToGrid(totalBox.vmin, voxelSize);
  totalBox.vmax = AlignOriginToGrid(totalBox.vmax, voxelSize);
  totalBox.vmax += 2.0f *Vec3f(voxelSize, voxelSize, voxelSize);
  totalBox.vmin -= 2.0f *Vec3f(voxelSize, voxelSize, voxelSize);

  // one origin convention everywhere: outOrigin is the min corner of voxel
  // (0,0,0), same as VoxConf::origin. it sits on the floor(x/voxelSize)
  // lattice that every per-item box is also snapped to, so the per-item index
  // offsets below are exact integers. no half voxel shifts anywhere.
  outOrigin = totalBox.vmin;

  Vec3u gridSize = ComputeGridSize(totalBox, voxelSize, 1);
  Array3D8u allVox;
  allVox.Allocate(gridSize[0], gridSize[1], gridSize[2]);
  allVox.Fill(0);

  for (const auto &inst : scene.instances) {
    unsigned itemIdx = inst.itemId;
    const RigidTransform &tran = inst.tran;

    TrigMesh transformedMesh = MakeTransformedMesh(scene.items[itemIdx].mesh, tran);
    Box3f bbox = ComputeBBox(transformedMesh.v);
    bbox.vmin = AlignOriginToGrid(bbox.vmin, voxelSize);
    bbox.vmax = AlignOriginToGrid(bbox.vmax, voxelSize);
    bbox.vmax += Vec3f(voxelSize, voxelSize, voxelSize);
    bbox.vmin -= 3.0f*Vec3f(voxelSize, voxelSize, voxelSize);

    VoxConf conf;
    conf.origin = bbox.vmin;
    conf.unit = {voxelSize, voxelSize, voxelSize};
    conf.gridSize = ComputeGridSize(bbox, voxelSize, 1);

    Array3D8u itemVox;
    itemVox.Allocate(conf.gridSize, 0);
    VoxelizeMesh(transformedMesh, itemVox, conf);
    FloodOutside8u(itemVox, 1, 2);

    Vec3i offset = RoundVec((1.0f / voxelSize) * (conf.origin - outOrigin));

    for (unsigned z = 0; z < conf.gridSize[2]; z++) {
      int gz = int(z) + offset[2];
      if (gz < 0 || (unsigned)gz >= gridSize[2]) continue;
      for (unsigned y = 0; y < conf.gridSize[1]; y++) {
        int gy = int(y) + offset[1];
        if (gy < 0 || (unsigned)gy >= gridSize[1]) continue;
        for (unsigned x = 0; x < conf.gridSize[0]; x++) {
          if (itemVox(x, y, z) != 0) {
            int gx = int(x) + offset[0];
            if (gx >= 0 && (unsigned)gx < gridSize[0]) {
              allVox((unsigned)gx, (unsigned)gy, (unsigned)gz) = 1;
            }
          }
        }
      }
    }
  }

  return allVox;
}

TrigMesh ComputeShrinkWrapMesh(PackingScene &scene, float shrinkRadius,
                               float voxelSize) {
  TrigMesh outMesh;
  Vec3f origin;
  Array3D8u itemsVox = VoxelizeItems(scene, voxelSize, origin);

  Array3D<short> dist;
  float distUnit = 0.01f;
  ComputeShrinkWrapDistField(itemsVox, voxelSize, shrinkRadius, dist, distUnit);

  MarchingCubes(dist, 0.0f, distUnit, voxelSize, origin, &outMesh);

  LOGI("shrinkwrap: " << outMesh.GetNumTrigs() << " triangles, radius "
                       << shrinkRadius << " cm, voxel size " << voxelSize << " cm\n");
  return outMesh;
}

void SaveShrinkWrapMesh(PackingScene &scene, const std::string &filename,
                        float shrinkRadius, float voxelSize) {
  TrigMesh mesh = ComputeShrinkWrapMesh(scene, shrinkRadius, voxelSize);
  mesh.SaveObj(filename);
  LOGI("saved shrinkwrap mesh to " << filename << "\n");
}
