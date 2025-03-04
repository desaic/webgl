#include <iostream>

#include "FEMApp.hpp"
#include "UIConf.h"
#include "UILib.h"

void ShowGLInfo(UILib& ui, int label_id) {
  std::string info = ui.GetGLInfo();
  ui.SetLabelText(label_id, info);
}

int main(int argc, char** argv) {
  UILib ui;
  FemApp app(&ui);
  ui.SetFontsDir("./fonts");
  ui.SetWindowSize(1280, 800);
  int statusLabel = ui.AddLabel("status");
  ui.Run();
  const unsigned PollFreqMs = 20;
  while (ui.IsRunning()) {
    app.Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
  return 0;
}
