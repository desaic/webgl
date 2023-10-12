#pragma once

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "Array2D.h"
#include "GLRender.h"
#include "GLTex.h"

namespace ImGui {
class FileBrowser;
};
struct ImGuiIO;
struct GLFWwindow;

class UIWidget {
 public:
  virtual void Draw() {}
  int GetIndex() const {}

 private:
  ///widgets are drawn in order indicated by index_
  int index_ = 0;
};

class Button : public UIWidget {
 public:
  void Draw() override;
  std::function<void()> _onClick;
  // display text+##id
  std::string _text;
  int _id = 0;
  std::string GetUniqueString() const;
  bool sameLine_ = false;
};

class Label : public UIWidget {
 public:
  std::string _text;
  void Draw() override;
};

class CheckBox : public UIWidget {
 public:
  CheckBox(const std::string& label, bool initVal)
      : label_(label), b_(initVal) {}
  std::string label_;
  void Draw() override;
  bool GetVal() const { return b_; }
 private:
  bool b_;
};

class PlotWidget : public UIWidget {
 public:
  PlotWidget(float initMin, float initMax, unsigned w, unsigned h,
             const std::string& title)
      : line_(10, 0),min_(initMin), max_(initMax),
  width_(w), height_(h), title_(title){}
  void SetData(const std::vector<float>& data) {
    std::lock_guard<std::mutex> lock(mtx_);
    line_ = data;
  }
  void Draw() override;
 private:
  std::vector<float> line_;
  float min_ = 0;
  float max_ = 100;
  unsigned width_ = 50;
  unsigned height_ = 50;
  std::string title_ = "";
  //for Draw and SetData
  mutable std::mutex mtx_;
};

/// <summary>
/// floating text uses the DrawList api instead of the normal ImGui api.
/// </summary>
class FloatingText {
 public:
  unsigned color_ = 0x808080ffu;
  std::string text_;  
  Vec2f pos_;
  bool draw_ = true;
};

class UILib {
 public:
  UILib();
  /// launch the ui thread and starts rendering and handling input.
  void Run();
  /// stops the ui thread.
  void Shutdown();

  void UILoop();
  bool IsRunning() const { return _running; }
  ~UILib() { Shutdown(); }

  /// Do not modify buttons in call back or it will deadlock.
  ///@return button id so that its callback or id may be modified in future.
  int AddButton(const std::string& text, const std::function<void()>& onClick,
                bool sameLine = false);

  int SetButtonCallback(int buttonId, const std::function<void()>& onClick);

  int AddCheckBox(const std::string& label, bool initVal);

  int AddLabel(const std::string& text);

  /// @return widget ID.
  int AddImage();

  int AddPlot(float initMin, float initMax,
              const std::string& title);

  int SetPlotData(int widgetId, const std::vector<float>& data);

  /// Can only be called after Init();
  /// If image is RGB, width is data.GetSize()[0]/3.
  int SetImageData(int imageId, const Array2D8u& data, int numChannels);

  int SetImageData(int imageId, const Array2D4b& image);

  int SetImageScale(int id, float scale);

  size_t NumImages() const { return _images.size(); }

  void SetFontsDir(const std::string& dir) { _fontsDir = dir; }

  void SetTitle(const std::string& s) { _title = s; }

  void SetLabelText(int id, const std::string & text);

  /// <summary>
  /// 
  /// </summary>
  /// <param name="text"></param>
  /// <returns>id of the te</returns>
  int AddFloatingText(const std::string& text, unsigned color);
  
  void SetFloatingText(int id, const std::string& text);

  void SetFloatingTextPosition(int id, Vec2f pos);

  bool GetCheckBoxVal(int id) const;
  
  void SetImageWindowTitle(const std::string& s);

  std::string GetGLInfo() const { return _glRender.GLInfo(); }

  void SetShaderDir(const std::string& dir) { _shaderDir = dir; }

  /// @return mesh index
  int AddMesh(std::shared_ptr<TrigMesh> mesh);

  void SetWindowSize(unsigned w, unsigned h);
 private:

  void DrawImages();
  void DrawFloatingTexts();
  int LoadFonts(ImGuiIO& io);
  int LoadShaders();
  std::thread _uiThread;
  std::array<float, 4> _clearColor = {0.5f, 0.5f, 0.5f, 1};
  GLFWwindow* _window = nullptr;
  int _width = 2278;
  int _height = 900;

  mutable std::mutex _widgetsLock;
  std::vector<std::shared_ptr<UIWidget> > uiWidgets_;

  std::mutex floatingTextMutex_;
  std::vector<FloatingText> floatingTexts_;

  std::mutex _imagesLock;
  std::vector<GLTex> _images;

  std::shared_ptr<ImGui::FileBrowser> _fileBrowser;

  std::string _fontsDir = "./fonts";
  std::string _shaderDir = "./glsl/";
  std::string _title;

  std::string imageWindowTitle_="Images";
  bool _running = false;


  std::mutex glLock_;
  GLRender _glRender;
};
