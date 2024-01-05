
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"
#include "cpu_voxelizer.h"
#include "MeshUtil.h"
#include "ImageIO.h"
#include "ImageUtils.h"
#include "TrigMesh.h"
#include "UIConf.h"
#include "UILib.h"
#include "lodepng.h"
#include "StringUtil.h"
#include "Timer.h"
#include "ZRaycast.h"
#include "RaycastConf.h"
#include "TestNanoVdb.h"

void OnChangeDir(std::string dir, UIConf& conf) {
  conf.workingDir = dir;
  conf.Save();
}

void LogToUI(const std::string & str, UILib&ui, int statusLabel) {
  ui.SetLabelText(statusLabel, str);
}
void ShowGLInfo(UILib& ui, int label_id) {
  std::string info = ui.GetGLInfo();
  ui.SetLabelText(label_id, info);
}

int main(int, char**) {
  UILib ui;
  ui.SetFontsDir("./fonts");
  ui.SetWindowSize(1280, 800);

  int buttonId = ui.AddButton("GLInfo", {});
  int gl_info_id = ui.AddLabel(" ");
  std::function<void()> showGLInfoFunc =
      std::bind(&ShowGLInfo, std::ref(ui), gl_info_id);
  ui.SetButtonCallback(buttonId, showGLInfoFunc);
  ui.SetWindowSize(1280, 800);

  int statusLabel = ui.AddLabel("status");

  ui.Run();
  
  const unsigned PollFreqMs = 20;


  while (ui.IsRunning()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
  return 0;
}
