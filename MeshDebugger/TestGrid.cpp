#include "Array3D.h"
#include "Array3DUtils.h"
#include "BBox.h"
#include "cpu_voxelizer.h"
#include "Lattice.h"
#include "Grid3Df.h"
#include "VoxIO.h"
#include "MeshUtil.h"
#include "ZRaycast.h"
#include <deque>
#include <fstream>
#include <functional>

slicer::Grid3Df MakeOctetUnitCell() {
  slicer::Grid3Df cell;
  Vec3f cellSize(6.0f);
  cell.voxelSize = Vec3f(0.1f);
  unsigned n = cellSize[0] / cell.voxelSize[0];
  // distance values are defined on vertices instead of voxel centers
  Vec3u gridSize(n + 1);
  cell.Allocate(gridSize, 1000);
  cell.Array() = slicer::MakeOctetCell(cell.voxelSize, cellSize, gridSize);
  return cell;
}

struct VoxFun : public VoxCallback {
  virtual void operator()(unsigned x, unsigned y, unsigned z, size_t trigIdx) {
    if (fun) {
      fun(x, y, z, trigIdx);
    }
  }
  std::function<void(unsigned, unsigned, unsigned, size_t)> fun;
};

void MakeAcousticLattice() {
  slicer::Grid3Df cell = MakeOctetUnitCell();
  std::string meshFile = "F:/meshes/acoustic/pyramid.obj";
  TrigMesh pyramid;
  pyramid.LoadObj("meshFile");
  BBox box;
  ComputeBBox(pyramid.v, box);
  Array3D8u vox;
  VoxFun callback;
  Array3D8u voxGrid;
  voxconf conf;
  const float h = 0.3f;
  conf.unit = Vec3f(h, h, h);
  Vec3f size = box.vmax - box.vmin;

  conf.gridSize = Vec3u(unsigned(size[0] / h + 2), unsigned(size[1] / h + 2),
                        unsigned(size[2] / h + 2));
  conf.origin = box.vmin;

  voxGrid.Allocate(conf.gridSize, 0);
  callback.fun = [&voxGrid, size](unsigned x, unsigned y, unsigned z,
                                  size_t tIdx) {
    const Vec3u sized = voxGrid.GetSize();
    if (x < size[0] && y < size[1] && z < size[2]) {
      voxGrid(x, y, z) = 1;
    }
  };
  cpu_voxelize_mesh(conf, &pyramid, callback);
}

static Array3D8u GetBinaryGrid(const Array3Df& grid, Vec3f voxSize,
                               float thickness, float h) {
  Array3D8u binGrid;
  Vec3f cellSize((grid.GetSize()[0] - 1) * voxSize[0],
                 (grid.GetSize()[1] - 1) * voxSize[1],
                 (grid.GetSize()[2] - 1) * voxSize[2]);
  Vec3u size(cellSize[0] / h + 0.5f, cellSize[1] / h + 0.5f,
             cellSize[1] / h + 0.5f);
  binGrid.Allocate(size, 0);
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        float dist = 0;
        Vec3f coord((x + 0.5f) * h, (y + 0.5f) * h, (z + 0.5f) * h);
        dist = TrilinearInterp(grid, coord, voxSize);
        if (dist <= thickness) {
          binGrid(x, y, z) = 1;
        }
      }
    }
  }
  return binGrid;
}

void FlipGridX(Array3D8u& grid) {
  Vec3u size = grid.GetSize();
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0] / 2; x++) {
        uint8_t tmp = grid(x, y, z);
        grid(x, y, z) = grid(size[0] - x - 1, y, z);
        grid(size[0] - x - 1, y, z) = tmp;
      }
    }
  }
}

void FlipGridXY(Array3D8u& grid) {
  Vec3u size = grid.GetSize();
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = y + 1; x < size[0]; x++) {
        uint8_t tmp = grid(x, y, z);
        grid(x, y, z) = grid(y, x, z);
        grid(y, x, z) = tmp;
      }
    }
  }
}

void MakeShearXGrid() {
  slicer::Grid3Df cell;
  Vec3f cellSize(3.5f);

  cell.voxelSize = Vec3f(0.05f);
  unsigned n = cellSize[0] / cell.voxelSize[0];
  // distance values are defined on vertices instead of voxel centers
  Vec3u gridSize(n + 1);
  cell.Allocate(gridSize, 1000);
  cell.Array() = slicer::MakeShearXCell(cell.voxelSize, cellSize, gridSize);

  float h = 0.064;
  Array3D8u binGrid = GetBinaryGrid(cell.Array(), cell.voxelSize, 0.3, h);
  FlipGridX(binGrid);
  FlipGridXY(binGrid);
  SaveVoxTxt(binGrid, h, "F:/meshes/lattice_fablet/shearmy.txt");
}

void SaveVolMesh() {
  const std::string volFile = "F:/meshes/bcc/102.vol";
  std::string outFile = "F:/meshes/bcc/102.obj";
  std::ifstream in(volFile, std::fstream::binary);
  Vec3u size;
  in.read((char*)(&size), 12);
  Array3D8u grid(size[0], size[1], size[2]);
  size_t bytes = size[0] * size_t(size[1]) * size[2];
  in.read((char*)(grid.DataPtr()), bytes);
  float VoxRes[3] = {0.1, 0.1, 0.1};

  Array3D8u expanded(size[0] * 8,size[1], size[2]);
  for (size_t i = 0; i < grid.GetData().size(); i++) {
    uint8_t val = grid.GetData()[i];
    for (size_t j = 0; j < 8; j++) {
      size_t outIdx = i * 8 + j;
      uint8_t outVal = (val >> j) & 1;
      expanded.GetData()[outIdx] = outVal;
    }
  }
  SaveVolAsObjMesh(outFile, expanded, VoxRes, 1);

}

static bool InBound(int x, int y, int z, const Vec3u& size) {
  return x >= 0 && y >= 0 && z >= 0 && x < int(size[0]) && y < int(size[1]) &&
         z < int(size[2]);
}

static size_t LinearIdx(unsigned x, unsigned y, unsigned z, const Vec3u& size) {
  return x + y * size_t(size[0]) + z * size_t(size[0] * size[1]);
}

static Vec3u GridIdx(size_t l, const Vec3u& size) {
  unsigned x = l % size[0];
  unsigned y = (l % (size[0] * size[1])) / size[0];
  unsigned z = l / (size[0] * size[1]);
  return Vec3u(x, y, z);
}

//+1 to all existing values. marks all voxels connected to seed as '0'. 
void FloodFromSeed(Vec3u seed, Array3D8u& grid) {
  Vec3u size = grid.GetSize();
  size_t linearSeed = LinearIdx(seed[0], seed[1], seed[2], size);
  std::deque<size_t> q(1, linearSeed);
  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                               {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  for (size_t i = 0; i < grid.GetData().size(); i++) {
    grid.GetData()[i]++;
  }

  while (!q.empty()) {
    size_t linearIdx = q.front();
    q.pop_front();
    Vec3u idx = GridIdx(linearIdx, size);
    grid(idx[0], idx[1], idx[2]) = 0;
    for (unsigned ni = 0; ni < NUM_NBR; ni++) {
      int nx = int(idx[0] + nbrOffset[ni][0]);
      int ny = int(idx[1] + nbrOffset[ni][1]);
      int nz = int(idx[2] + nbrOffset[ni][2]);
      if (!InBound(nx, ny, nz, size)) {
        continue;
      }
      unsigned ux = unsigned(nx), uy = unsigned(ny), uz = unsigned(nz);
      uint8_t nbrLabel = grid(ux, uy, uz);
      size_t nbrLinear = LinearIdx(ux, uy, uz, size);
      if (nbrLabel == 1) {
        grid(ux, uy, uz) = 0;
        q.push_back(nbrLinear);
      }
    }
  }
}

void VoxelizeUnitCell() {
  slicer::Grid3Df cell = MakeOctetUnitCell();
  std::string meshFile = "F:/meshes/bcc/IBC_102.stl";
  float gridLengthMM = 3;
  Vec3f gridOrigin(-1.5f, -1.5f, -1.5f);
  TrigMesh latt;
  latt.LoadStl(meshFile);
  Array3D8u vox;
  VoxFun callback;
  Array3D8u voxGrid;
  voxconf conf;
  const float h = 0.03f;
  conf.unit = Vec3f(h, h, h);

  conf.gridSize = Vec3u(unsigned(gridLengthMM / h), unsigned(gridLengthMM / h),
                        unsigned(gridLengthMM / h));
  conf.origin = gridOrigin;

  voxGrid.Allocate(conf.gridSize, 0);
  callback.fun = [&voxGrid](unsigned x, unsigned y, unsigned z,
                                  size_t tIdx) {
    const Vec3u& size = voxGrid.GetSize();
    if (x < size[0] && y < size[1] && z < size[2]) {
      voxGrid(x, y, z) = 1;
    }
  };  
  cpu_voxelize_mesh(conf, &latt, callback);
  Vec3u size = conf.gridSize;
  FloodFromSeed(Vec3u(size[0]/2, 0,size[1]/2), voxGrid);
  for (auto& c : voxGrid.GetData()) {
    if (c > 0) {
      c = 1;
    }
  }
  //SaveVolAsObjMesh("F:/meshes/bcc/cell_vol.obj", voxGrid, (float*)(&conf.unit), 1);
  SaveVoxTxt(voxGrid, h, "F:/meshes/bcc/ibc_102.txt", Vec3f(0.21,0.21,0.21));
}

void VoxelizeRaycast() {

  slicer::Grid3Df cell = MakeOctetUnitCell();
  std::string meshFile = "F:/meshes/bcc/IBC_102.stl";
  float gridLengthMM = 3;
  Vec3f gridOrigin(-gridLengthMM, -gridLengthMM, -gridLengthMM);
  gridOrigin *= 2;
  TrigMesh latt;
  latt.LoadStl(meshFile);
  Array3D8u vox;
  const float h = 0.03f;
  Vec3u gridSize = Vec3u(unsigned(gridLengthMM / h));
  Vec3u startIndex = gridSize + gridSize / 2u;
  ZRaycast raycast;
  RaycastConf rcConf;
  rcConf.voxelSize_ = Vec3f(h,h,h);
  BBox box;
  box.vmin = gridOrigin;
  box.vmax = -gridOrigin;
  rcConf.box_ = box;
  rcConf.ComputeCoarseRes();
  ABufferf abuf;
  int ret = raycast.Raycast(latt, abuf, rcConf);  
  
  float z0 = box.vmin[2];
  // expand abuf to vox grid
  vox.Allocate(gridSize, 0);
  for (unsigned y = 0; y < gridSize[1]; y++) {
    unsigned srcy = y + startIndex[1];
    for (unsigned x = 0; x < gridSize[0]; x++) {
      unsigned srcx = x + startIndex[0];
      const auto intersections = abuf(srcx, srcy);
      for (auto seg : intersections) {
        unsigned zIdx0 = seg.t0 / h + 0.5f;
        unsigned zIdx1 = seg.t1 / h + 0.5f;
        for (unsigned z = zIdx0; z < zIdx1; z++) {
          int dstz = int(z) - startIndex[2]; 
          if (dstz < 0 || dstz >= int(gridSize[2])) {
            continue;
          }
          vox(x, y, unsigned(dstz)) = 1;
        }
      }
    }
  }
  SaveVolAsObjMesh("F:/meshes/bcc/cell_ray_vol.obj", vox, (float*)(&rcConf.voxelSize_), 1);
  SaveVoxTxt(vox, h, "F:/meshes/bcc/ibc_102.txt");
}
