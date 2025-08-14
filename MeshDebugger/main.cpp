#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>

#include "CadApp.hpp"
#include "CanvasApp.h"
#include "UVApp.h"
#include "InflateApp.h"
#include "PackApp.hpp"
#include "UIConf.h"
#include "UILib.h"
#include "VoxApp.h"
#include "HeadApp.hpp"

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

void RunCadApp() {
  UILib ui;
  CadApp app;
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
    app.RefreshUI();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
}

void RunPackApp() {
  UILib ui;
  PackApp app;
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

extern void MapDrawingToThickness();

void RunHeadApp() {
  UILib ui;
  HeadApp app;
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

void RunUVApp() {
  UILib ui;
  ui.Run();
  DebugUV(ui);

  const unsigned PollFreqMs = 20;
  while (ui.IsRunning()) {
    // main thread not doing much
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
}

extern void MeshHeightMap();
extern void MapDrawingToThickness();
extern void TestSDF();

extern void SaveVolMeshAsSurf();

extern void JsToObj();

extern void TestSurfRaySample();

extern void VoxelizeUnitCell();
extern void VoxelizeRaycast();

extern void TestGrin();

int main(int argc, char** argv) {
  //RunUVApp();
  // RunCanvasApp();
  // RunInflateApp();
  // RunPackApp();
  // VoxApp();
  // RunHeadApp();
  // MeshHeightMap();
  // MapDrawingToThickness();
  //TestSDF();
  // JsToObj(); 
  //SaveVolMeshAsSurf();
  //TestSurfRaySample();
  
  TestGrin();

  //VoxelizeUnitCell();
  //VoxelizeRaycast();
  return 0;
}
