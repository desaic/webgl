#pragma once

#include <thread>
#include <array>
#include <functional>
#include <mutex>
#include <vector>
#include <memory> 

#include "GLTex.h"
#include "Array2D.h"
#include "GLRender.h"

namespace ImGui {
class FileBrowser;
};
struct ImGuiIO;
struct GLFWwindow;

struct Button {
  std::function<void()>_onClick;
  // display text+##id
  std::string _text;
  int _id = 0;
  std::string GetUniqueString() const;
};

struct Label {
  std::string _text;
};


class UILib {

public:
  UILib();
  /// launch the ui thread and starts rendering and handling input.
  void Run();
  /// stops the ui thread.
  void Shutdown();

  void UILoop();

  ~UILib() {
    Shutdown();
  }

  ///Do not modify buttons in call back or it will deadlock.
  ///@return button id so that its callback or id may be modified in future.
  int AddButton(const std::string &text, const std::function<void()> & onClick);

  int AddLabel(const std::string &text);

  /// @return widget ID.
  int AddImage();
  /// Can only be called after Init();
  /// If image is RGB, width is data.GetSize()[0]/3.
  int SetImageData(int imageId, const Array2D8u &data,
    int numChannels);
  
  int SetImageData(int imageId, const Array2D4b& image);  

  void SetFontsDir(const std::string& dir) { _fontsDir = dir; }

private:

  void DrawImages();
  void HandleButtons();
  void DrawLabels();
  int LoadFonts(ImGuiIO&io);

  std::thread _uiThread;
  std::array<float, 4> _clearColor = {0.5f,0.5f,0.5f,1};
  GLFWwindow* _window = nullptr;
  int _width=1280;
  int _height=720;
  //button ids are auto incremented to avoid conflicts
  std::mutex _buttonsLock;
  std::vector<Button> _buttons;
  std::mutex _labelsLock;
  std::vector<Label> _labels;
  std::mutex _imagesLock;
  std::vector<GLTex> _images;

  std::shared_ptr<ImGui::FileBrowser> _fileBrowser;

  std::string _fontsDir="./fonts";

  /// for 3D scene.
  GLRender glRender;
};
