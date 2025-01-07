#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "ImageIO.h"
#include "CadApp.hpp"
#include "CanvasApp.h"
#include "PackApp.hpp"
#include "VoxApp.h"
#include "InflateApp.h"
#include "UIConf.h"
#include "UILib.h"

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

int main(int argc, char** argv) {
  //RunCanvasApp();
  // RunInflateApp();
  RunPackApp();
  // VoxApp();
  return 0;
}
