
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"

#include "cad_app.h"
#include "VoxApp.h"
#include "UIConf.h"
#include "UILib.h"
#include "TriangulateContour.h"

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

int main(int argc, char**argv) {
  if (argc > 1 && argv[1][0] == 'c') {
    CadApp();
  } else {
    VoxApp();
  }
  return 0;
}
