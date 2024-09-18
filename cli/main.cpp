
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
#include "meshutil.h"
#include "VoxIO.h"

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
  scale = std::min(maxScale, (1.0f+std::abs(dist) * 0.05f));
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

Array3D8u removePadding(const Array3D8u& input, uint8_t minVal) {
  Array3D8u out;
  Vec3u gridSize = input.GetSize();
  Vec3u mn= gridSize, mx(0);
  for (unsigned z = 0; z < gridSize[2]; z++) {
    for (unsigned y = 0; y < gridSize[1]; y++) {
      for (unsigned x = 0; x < gridSize[0]; x++) {
        if (input(x, y, z) < minVal) {
          continue;
        }
        mn[0] = std::min(mn[0], x);
        mn[1] = std::min(mn[1], y);
        mn[2] = std::min(mn[2], z);
        mx[0] = std::max(mx[0], x);
        mx[1] = std::max(mx[1], y);
        mx[2] = std::max(mx[2], z);
      }
    }
  }
  mn = mn + Vec3u(2, 2, 0);
  mx = mx - Vec3u(2, 2, 0);
  Vec3u outSize = mx - mn + Vec3u(1u);
  out.Allocate(outSize, minVal);
  for (unsigned z = 0; z < outSize[2]; z++) {
    for (unsigned y = 0; y < outSize[1]; y++) {
      for (unsigned x = 0; x < outSize[0]; x++) {
        out(x, y, z) = input(x + mn[0], y + mn[1], z + mn[2]);
      }
    }
  }
  return out;
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

  Vec3u dfSize = df->dist.GetSize();
  // std::shared_ptr<AdapDF> lattice= std::make_shared<AdapSDF>();
  //lattice->Allocate(dfSize[0], dfSize[1], dfSize[2]);
  //lattice->voxSize = df->voxSize;
  //lattice->origin = df->origin;
  //lattice->distUnit = df->distUnit;
  float h = df->voxSize;
  float scale = 1.0f;
  Array3D8u voxGrid(dfSize[0] - 1, dfSize[1]- 1, dfSize[2]-1);
  voxGrid.Fill(1);
  float isoLevel = 0.92;
  for (unsigned z = 0; z < dfSize[2]; z++) {
    for (unsigned y = 0; y < dfSize[1]; y++) {
      for (unsigned x = 0; x < dfSize[0]; x++) {
        Vec3f coord(x * h, y * h, z * h);
        //voxel centers are offset from distance field value vertices
        coord += df->origin + 0.5f*Vec3f(h);
        float coarseDist = df->GetCoarseDist(coord);
        if (coarseDist > 0) {
          continue;
        }
        coord = DFCoord(coord, scale, df.get());        
        Vec3f cellX = CellCoord(coord, cellSize);
        float dist = scale * cell.Trilinear(cellX);        
        float level = isoLevel;
        if (coarseDist > -0.5f) {
          level = 1.78;
        }
        if (dist <= level) {
          voxGrid(x, y, z) = 2;
        }
        //lattice->dist(x, y, z) = short(dist / df->distUnit);
      }
    }
  }
  //TrigMesh surf;
  //SDFImpAdap dfImp(lattice);
  //float beamThickness = 0.9;
  //dfImp.MarchingCubes(beamThickness, &surf);
  //surf.SaveObj("F:/meshes/acoustic/latticeSurf2.obj");
  //Vec3f voxSize(h);
  //SaveVolAsObjMesh("F:/meshes/acoustic/vox.obj", voxGrid, (float*)(&voxSize), (float*)(&df->origin),
  //                 2);
  voxGrid = removePadding(voxGrid, 2);
  SaveVoxTxt(voxGrid, h, "F:/meshes/acoustic/octet_pyramid_skin.txt");
}

struct FunCb : public VoxCallback {
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) override;
  
};

void VoxelizeHoneyComb(std::string meshFile) {

  TrigMesh m;
  m.LoadStl(meshFile);
  BBox box;
  ComputeBBox(m.v, box);
  Vec3f boxCenter = 0.5f * (box.vmax + box.vmin);
  for (size_t i = 0; i < m.GetNumVerts(); i++) {
    m.v[3 * i] -= boxCenter[0];
    m.v[3 * i + 1] -= boxCenter[1];
    m.v[3 * i + 2] -= boxCenter[2];
  }

  box.vmin = box.vmin - boxCenter;
  box.vmax = box.vmax - boxCenter;
  float voxRes = 0.1;
  Vec3f unit = Vec3f(voxRes, voxRes, voxRes);

  box.vmin[1] += 0.4;
  box.vmax[1] -= 0.4;

  Vec3f count = (box.vmax - box.vmin) / unit;
  Vec3u gridSize = Vec3u(count[0] + 1, count[1] + 1, count[2] + 1);

  // 0 for void and 1 for wax therefore +2.
  uint8_t mat0 = 2;
  Array3D8u grid;
  grid.Allocate(gridSize, 1);
  voxconf conf;
  conf.gridSize = gridSize;
  conf.origin = box.vmin;
  conf.unit = 0.1f;

}

int main(int argc, char** argv) {  
  //MakeAcousticLattice();
  VoxelizeHoneyComb("F:/meshes/acoustic/honeyCombCell.stl");
  return 0;
}
