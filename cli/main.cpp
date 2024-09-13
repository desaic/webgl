
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
#include "Inflate.h"
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
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  InflateConf conf;
  conf.voxResMM = 0.2;
  conf.thicknessMM = 0.5;
  std::shared_ptr<AdapDF> udf = ComputeOutsideDistanceField(conf, mesh);
  //TrigMesh surf;
  //SDFImpAdap dfImp(udf);
  //dfImp.MarchingCubes(conf.thicknessMM, &surf);
  //surf.SaveObj("F:/meshes/acoustic/pyramidSurf1.obj");

}

int main(int argc, char** argv) {  
  MakeAcousticLattice();

  return 0;
}
