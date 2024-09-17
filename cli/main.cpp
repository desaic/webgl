
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

slicer::Grid3Df MakeOctetUnitCell(Vec3f cellSize) {
  slicer::Grid3Df cell;
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

// finer and denser towards center by scaling input coordinates
// to larger numbers. scale = 1 near boundary.
Vec3f DFCoord(Vec3f x, float & scale, const AdapDF* df) {
  float maxScale = 2;
  float dist = df->GetCoarseDist(x);
  scale = std::min(maxScale, (1.0f+std::abs(dist) * 0.12f));
  Vec3f y = x * scale;
  return y;
}

Vec3f CellCoord(const Vec3f & x, const Vec3f & cellSize)
{
  Vec3f y;
  for (unsigned d = 0; d < 3; d++) {
    int idx = int(x[d] / cellSize[d]);
    if (idx * cellSize[d] > x[d]) {
      idx--;
    }
    y[d] = x[d] - cellSize[d] * idx;

  }
  return y;
}

void MakeAcousticLattice() {
  Vec3f cellSize(8);
  slicer::Grid3Df cell = MakeOctetUnitCell(cellSize);
  std::string meshFile = "F:/meshes/acoustic/pyramid.obj";
  TrigMesh mesh;
  mesh.LoadObj(meshFile);
  InflateConf conf;
  conf.voxResMM = 0.2;
  conf.thicknessMM = 1;  
  std::shared_ptr<AdapDF> df = ComputeFullDistanceField(conf, mesh);
  //distance field for lattice.
  std::shared_ptr<AdapDF> lattice= std::make_shared<AdapSDF>();
  Vec3u dfSize = df->dist.GetSize();
  lattice->Allocate(dfSize[0], dfSize[1], dfSize[2]);
  lattice->voxSize = df->voxSize;
  lattice->origin = df->origin;
  lattice->distUnit = df->distUnit;
  float h = df->voxSize;
  float scale = 1.0f;
  for (unsigned z = 0; z < dfSize[2]; z++) {
    for (unsigned y = 0; y < dfSize[1]; y++) {
      for (unsigned x = 0; x < dfSize[0]; x++) {
        Vec3f coord(x * h, y * h, z * h);
        coord += df->origin;
        if (df->GetCoarseDist(coord) > 0) {
          continue;
        }
        coord = DFCoord(coord, scale, df.get());        
        Vec3f cellX = CellCoord(coord, cellSize);
        float dist = cell.Trilinear(cellX);
        lattice->dist(x, y, z) = short(scale * dist / df->distUnit);
      }
    }
  }
  TrigMesh surf;
  SDFImpAdap dfImp(lattice);
  float beamThickness = 0.9;
  dfImp.MarchingCubes(beamThickness, &surf);
  surf.SaveObj("F:/meshes/acoustic/latticeSurf2.obj");

}

int main(int argc, char** argv) {  
  MakeAcousticLattice();

  return 0;
}
