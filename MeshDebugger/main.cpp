
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"

#include "ImageIO.h"
#include "cad_app.h"
#include "CanvasApp.h"
#include "VoxApp.h"
#include "InflateApp.h"
#include "UIConf.h"
#include "UILib.h"
#include "TriangulateContour.h"
#include "ThreadPool.h"
#include "cpu_voxelizer.h"
#include "Lattice.h"
#include "Grid3Df.h"

#include <functional>

void VoxApp() {
  UILib ui;
  ConnectorVox app;
  app.Init(&ui);
  ui.SetFontsDir("./fonts");

  int statusLabel = ui.AddLabel("status");

  ui.Run();

  const unsigned PollFreqMs = 20;

  while (ui.IsRunning()) {
    app.Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
}

void RunInflateApp() {
  UILib ui;
  InflateApp app;
  app.Init(&ui);
  ui.SetFontsDir("./fonts");
  int statusLabel = ui.AddLabel("status");
  ui.Run();
  const unsigned PollFreqMs = 20;
  while (ui.IsRunning()) {
    app.Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
}

void TestCDT() {
  TrigMesh mesh;
  std::vector<float> v(8);
  mesh = TriangulateLoop(v.data(), v.size() / 2);
  mesh.SaveObj("F:/dump/loop_trigs.obj");
}

void CadApp() {
  UILib ui;
  cad_app app;
  app.Init(&ui);
  ui.SetFontsDir("./fonts");
  int statusLabel = ui.AddLabel("status");
  ui.Run();

  const unsigned PollFreqMs = 20;

  while (ui.IsRunning()) {
    app.Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
}

void RunCanvasApp() {
  
    UILib ui;
    CanvasApp app;
    app.conf._confFile = "./canvas_conf.json";
    app.Init(&ui);
    ui.SetFontsDir("./fonts");

    int statusLabel = ui.AddLabel("status");

    ui.Run();

    const unsigned PollFreqMs = 20;

    while (ui.IsRunning()) {
      app.Refresh();
      std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
    }
  
}

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

void CenterMeshes(const std::string &buildDir) {
  for (int i = 0; i < 18; i++) {
    std::string modelDir = buildDir + std::to_string(i) + "/models";
    TrigMesh m;
    std::string meshFile = modelDir + "/0.stl";
    m.LoadStl(meshFile);
    BBox box;
    ComputeBBox(m.v, box);
    Vec3f center = 0.5f * (box.vmin + box.vmax);
    for (size_t j = 0; j < m.v.size(); j += 3) {
      m.v[j] -= center[0];
      m.v[j + 1] -= center[1];
      m.v[j + 2] -= center[2];
    }
    m.SaveStl(meshFile);
  }
}

int main(int argc, char** argv) {
  
    //MakeAcousticLattice();
    // MakeXYPairs();
    //RunCanvasApp();
  RunInflateApp();
    //return 0;
    // if (argc > 1 && argv[1][0] == 'c') {
    //   CadApp();
    // } else {
    //VoxApp();
    //}
    return 0;
}
