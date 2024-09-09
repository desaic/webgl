
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "Array3DUtils.h"
#include "BBox.h"
#include "Grid3Df.h"
#include "Lattice.h"
#include "cpu_voxelizer.h"
#include "SDFMesh.h"
#include "AdapUDF.h"
#include "AdapSDF.h"

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
  pyramid.LoadObj(meshFile);
  BBox box;
  ComputeBBox(pyramid.v, box);
  Array3D8u vox;
  VoxFun callback;
  Array3D8u voxGrid;
  voxconf conf;
  const float h = 0.2f;
  conf.unit = Vec3f(h, h, h);
  Vec3f size = box.vmax - box.vmin;

  conf.gridSize = Vec3u(unsigned(size[0] / h + 2), unsigned(size[1] / h + 2),
                        unsigned(size[2] / h + 2));
  conf.origin = box.vmin;

  voxGrid.Allocate(conf.gridSize, 0);
  callback.fun = [&voxGrid](unsigned x, unsigned y, unsigned z, size_t tIdx) {
    const Vec3u size = voxGrid.GetSize();
    if (x < size[0] && y < size[1] && z < size[2]) {
      voxGrid(x, y, z) = 1;
    }
  };
  cpu_voxelize_mesh(conf, &pyramid, callback);

  auto sdfImp = std::make_shared<SDFImpAdap>(std::make_shared<AdapUDF>());
  SDFMesh sdf(sdfImp);
  sdf.SetBand(10);
  sdf.SetMesh(&pyramid);
  sdf.Compute();
  TrigMesh surf;
  sdf.MarchingCubes(1, &surf);
  surf.SaveObj("F:/dump/march.obj");
}

void InflateMesh(const std::vector<float> & v, const std::vector<unsigned> & t) {
  TrigMesh mesh;
  mesh.v = v;
  mesh.t = t;
  BBox box;
  ComputeBBox(mesh.v, box);
  Vec3f boxSize = box.vmax - box.vmin;
  float maxLen =boxSize[0];
  for (unsigned d = 1; d < 3; d++) {
    maxLen = std::max(maxLen, boxSize[d]);
  }
  const unsigned MAX_GRID_SIZE = 500;

  auto sdfImp = std::make_shared<SDFImpAdap>(std::make_shared<AdapUDF>());

  SDFMesh sdf(sdfImp);
  sdf.SetBand(10);
  sdf.SetMesh(&mesh);
  sdf.Compute();
  TrigMesh surf;
  sdf.MarchingCubes(1, &surf);
}

int main(int argc, char** argv) {
  MakeAcousticLattice();

  return 0;
}
