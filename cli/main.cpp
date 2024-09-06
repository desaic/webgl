
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"

#include "cpu_voxelizer.h"
#include "Grid3Df.h"
#include "Lattice.h"
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
    virtual void operator()(unsigned x, unsigned y, unsigned z,
                            size_t trigIdx) {
      if (fun) {
        fun(x, y, z, trigIdx);
      }
    }
    std::function<void(unsigned, unsigned, unsigned, size_t)> fun;
};

void MakeAcousticLattice() { slicer::Grid3Df cell = MakeOctetUnitCell();
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
    callback.fun = [&voxGrid](unsigned x, unsigned y, unsigned z, size_t tIdx) {
      const Vec3u size = voxGrid.GetSize();
      if (x < size[0] && y < size[1] && z < size[2]) {
        voxGrid(x, y, z) = 1;
      }
    };
    cpu_voxelize_mesh(conf, &pyramid, callback);

}

int main(int argc, char** argv) {
    MakeAcousticLattice();

    return 0;
}
