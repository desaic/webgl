
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

void ShowGLInfo(UILib& ui, int label_id) {
  std::string info = ui.GetGLInfo();
  ui.SetLabelText(label_id, info);
}

void TestCDT() { 
  TrigMesh mesh; 
  std::vector<float> v(8);
  mesh = TriangulateLoop(v.data(), v.size()/2);
  mesh.SaveObj("F:/dump/loop_trigs.obj");
}

int main(int, char**) {
  UILib ui;
  cad_app app;
  //ConnectorVox app;
  app.Init(&ui);
  ui.SetFontsDir("./fonts");

  int buttonId = ui.AddButton("GLInfo", {});
  int gl_info_id = ui.AddLabel(" ");
  std::function<void()> showGLInfoFunc =
      std::bind(&ShowGLInfo, std::ref(ui), gl_info_id);
  ui.SetButtonCallback(buttonId, showGLInfoFunc);

  int statusLabel = ui.AddLabel("status");

  ui.Run();
  
  const unsigned PollFreqMs = 20;

  while (ui.IsRunning()) {
    app.Refresh();
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
  return 0;
}
