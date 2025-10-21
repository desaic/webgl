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
#include "TrigMesh.h"
#include "BBox.h"

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
extern void TestGrinDiamond();
extern void CountVoxelsInVol();
extern void EstMass();

extern void RaycastMeshes();

extern void TestBLD();

void trim_leading_spaces(std::string& s) {
  // 1. Find the iterator pointing to the first character
  //    that is NOT a whitespace character (using std::isspace).
  auto it = std::find_if(s.begin(), s.end(),
                         [](unsigned char ch) { return !std::isspace(ch); });

  // 2. Erase the range of characters from the beginning (s.begin())
  //    up to the first non-space character (it).
  s.erase(s.begin(), it);
}

void MoveFiles() {
  std::string pairFile = "F:/meshes/skeleton/closest_matched_pairs.txt";
  std::ifstream in(pairFile);
  std::string line;
  std::string inDir = "F:/meshes/skeleton/right/";
  std::string outDir = "F:/meshes/skeleton/rightOut/";

  while (std::getline(in, line)) {
    if (line.size() < 3 || line[0] == '#') {
      continue;
    }
    std::stringstream ss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (std::getline(ss, token, ',')) {
      tokens.push_back(token);
    }
    std::string leftName, rightName;
    leftName = tokens[0];
    rightName = tokens[1];
    std::replace(leftName.begin(), leftName.end(), '.', '_');
    std::replace(rightName.begin(), rightName.end(), '.', '_');
    trim_leading_spaces(leftName);
    trim_leading_spaces(rightName);
    TrigMesh mesh;
    //scale 10x , center bounding box.
    std::string inFile = inDir + rightName + ".obj";
    mesh.LoadObj(inFile);
    if (mesh.v.empty()) {
      std::cout << "fucked up\n";
    }
    Box3f box = ComputeBBox(mesh.v);
    //Vec3f boxSize = box.vmax - box.vmin;
    Vec3f boxCenter = 0.5f * (box.vmax + box.vmin);
    mesh.translate(-boxCenter[0], -boxCenter[1], -boxCenter[2]);
    mesh.scale(10);
    std::string outFile = outDir + "r" + leftName + ".obj";
    mesh.SaveObj(outFile);
  }
}

int main(int argc, char** argv) {
 // MoveFiles();
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
  
  //TestGrin();
  //TestGrinDiamond();
  //EstMass();
  //CountVoxelsInVol();

  //VoxelizeUnitCell();
  //VoxelizeRaycast();

  //RaycastMeshes();
  TestBLD();
  return 0;
}
