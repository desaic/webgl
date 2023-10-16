#include "UILib.h"

#include <functional>
#include <iostream>
#include "imgui.h"
#include <Windows.h>
#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>  // Will drag system OpenGL headers
#include <fstream>

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imfilebrowser.h"
#include "Vec4.h"

static void glfw_error_callback(int error, const char* description) {
  std::cout << "Glfw Error " << error << " " << description << "\n";
}

UILib::UILib() {
  _fileBrowser = std::make_shared<ImGui::FileBrowser>();
}

int UILib::AddImage() {
  std::lock_guard<std::mutex> lock(_imagesLock);
  int imageId = int(_images.size());
  _images.push_back(GLTex());  
  return imageId;
}

int UILib::AddMesh(std::shared_ptr<TrigMesh> mesh) {
  std::lock_guard<std::mutex> lock(glLock_);
  int meshId = _glRender.AddMesh(mesh);  
  return meshId;
}

int UILib::AddButton(const std::string& text,
                     const std::function<void()>& onClick, bool sameLine) {  
  std::shared_ptr<Button> button = std::make_shared<Button>();
  button->_text = text;
  button->_onClick = onClick;
  button->sameLine_ = sameLine;

  std::lock_guard<std::mutex> lock(_widgetsLock);
  int id = int(uiWidgets_.size());
  button->_id = id;
  uiWidgets_.push_back(button);
  return id;
}

int UILib::AddCheckBox(const std::string& label, bool initVal) {
  std::shared_ptr<CheckBox> box = std::make_shared<CheckBox>(label, initVal);  
  std::lock_guard<std::mutex> lock(_widgetsLock);
  int id = int(uiWidgets_.size());
  uiWidgets_.push_back(box);
  return id;
}

int UILib::AddLabel(const std::string& text) {
  std::shared_ptr<Label> label = std::make_shared<Label>();
  label->_text = text;

  std::lock_guard<std::mutex> lock(_widgetsLock);
  int id = int(uiWidgets_.size());
  uiWidgets_.push_back(label);  
  return id;
}

int UILib::AddPlot(float initMin, float initMax, const std::string& title) {
  std::shared_ptr<PlotWidget> plot =
      std::make_shared<PlotWidget>(initMin, initMax, 150, 60, title);

  std::lock_guard<std::mutex> lock(_widgetsLock);
  int id = int(uiWidgets_.size());
  uiWidgets_.push_back(plot);
  return id;
}

int UILib::SetPlotData(int widgetId, const std::vector<float>& data) {
  std::lock_guard<std::mutex> lock(_widgetsLock);
  if (widgetId < 0 || widgetId >= uiWidgets_.size()) {
    return -1;
  }
  std::shared_ptr<PlotWidget> plot =
      std::dynamic_pointer_cast<PlotWidget>(uiWidgets_[size_t(widgetId)]);
  if (!plot) {
    return -2;
  }
  plot->SetData(data);
  return 0;
}

int UILib::SetButtonCallback(int id,
                             const std::function<void()>& onClick) {
  std::lock_guard<std::mutex> lock(_widgetsLock);
  if (id < 0 || id >= uiWidgets_.size()) {
    return -1;
  }
  std::shared_ptr<Button> button =
      std::dynamic_pointer_cast<Button>(uiWidgets_[size_t(id)]);
  if (!button) {
    return -2;
  }
  button->_onClick = onClick;
  return 0;
}

void UILib::SetLabelText(int id, const std::string& text) {
  std::lock_guard<std::mutex> lock(_widgetsLock);
  if (id < 0 || id >= uiWidgets_.size()) {
    return;
  }
  std::shared_ptr<Label> label = std::dynamic_pointer_cast<Label>(uiWidgets_[size_t(id)]);
  if (!label) {
    return;
  }
  label->_text = text;
}


int UILib::AddFloatingText(const std::string& text, unsigned color)
{
  std::lock_guard<std::mutex> lock(floatingTextMutex_);
  int id = int(floatingTexts_.size());
  FloatingText t ;
  t.text_ = text;
  t.color_ = color;
  floatingTexts_.push_back(t);
  return id;
}

void UILib::SetFloatingText(int id, const std::string &text) {
  std::lock_guard<std::mutex> lock(floatingTextMutex_);
  if (id < 0 || id >= floatingTexts_.size()) {
    return;
  }
  floatingTexts_[id].text_ = text;
}

void UILib::SetFloatingTextPosition(int id, Vec2f position) {
  std::lock_guard<std::mutex> lock(floatingTextMutex_);
  if (id < 0 || id >= floatingTexts_.size()) {
    return;
  }
  floatingTexts_[id].pos_ = position;
}

int UILib::SetImageData(int imageId, const Array2D8u& image, int numChannels) {
  std::lock_guard<std::mutex> lock(_imagesLock);
  if (imageId < 0 || imageId >= _images.size()) {
    return -1;
  }
  GLTex& d = _images[unsigned(imageId)];
  Vec2u size = image.GetSize();
  d._needAlloc = !(d.HasImage() && d._height == size[1] &&
                   d._width == size[0] && d._numChan == numChannels);
  d._drawHeight = size[1];
  if (numChannels >0) {
    d._drawWidth = size[0] / numChannels;
  }

  if (numChannels == 3) {
    // convert RGB to RGBA
    d._numChan = 4;
    d._width = size[0]/3*4;
    d._height = size[1];
    d._buf.resize(d._width * d._height);
    const auto& imgData = image.GetData();
    size_t dstIdx = 0;
    for (size_t i = 0; i < imgData.size(); i+=3) {
      d._buf[dstIdx] = imgData[i];
      d._buf[dstIdx + 1] = imgData[i + 1];
      d._buf[dstIdx + 2] = imgData[i + 2];
      d._buf[dstIdx + 3] = 255;
      dstIdx += 4;
    }
  } else {
    d._numChan = numChannels;
    d._width = size[0];
    d._height = size[1];
    d._buf = image.GetData();
  }
  d._needUpdate = true;
  return 0;
}

bool UILib::GetCheckBoxVal(int id) const {
  std::lock_guard<std::mutex> lock(_widgetsLock);
  if (id < 0 || id >= uiWidgets_.size()) {
    return false;
  }
  std::shared_ptr<CheckBox> box =
      std::dynamic_pointer_cast<CheckBox>(uiWidgets_[size_t(id)]);
  return box->GetVal();
}

void UILib::SetImageWindowTitle(const std::string& s) {
  std::lock_guard<std::mutex> lock(_widgetsLock);
  imageWindowTitle_ = s;
}

void Vec4bTo8u(const std::vector<Vec4b>& src, std::vector<uint8_t>& dst) {
  dst.resize(src.size() * 4);
  size_t dsti = 0;
  for (size_t srci = 0; srci < src.size(); srci++) {
    dst[dsti] = src[srci][0];
    dst[dsti + 1] = src[srci][1];
    dst[dsti + 2] = src[srci][2];
    dst[dsti + 3] = src[srci][3];
    dsti += 4;
  }
}

int UILib::SetImageData(int imageId, const Array2D4b& image) {
  std::lock_guard<std::mutex> lock(_imagesLock);
  if (imageId < 0 || imageId >= _images.size()) {
    return -1;
  }
  const unsigned inputChan = 4;
  GLTex& d = _images[unsigned(imageId)];
  Vec2u size = image.GetSize();
  d._needAlloc = !(d.HasImage() && d._height == size[1] &&
                   d._width == size[0] * inputChan && d._numChan == inputChan);
  d._drawHeight = size[1];
  d._drawWidth = size[0];  
  d._numChan = inputChan;
  d._width = size[0] * inputChan;
  d._height = size[1];
  Vec4bTo8u(image.GetData(), d._buf);
  d._needUpdate = true;
  return 0;
}

int UILib::SetImageScale(int id, float scale) {
  std::lock_guard<std::mutex> lock(_imagesLock);
  if (id < 0 || id >= _images.size()) {
    return -1;
  }
  _images[size_t(id)].renderScale_ = scale;
  return 0;
}

void UILib::DrawImages() {
  for (size_t i = 0; i < _images.size(); i++) {
    GLTex& img = _images[i];
    if (img._needUpdate) {
      img.UpdateTextureData();
      img._needUpdate = false;
    }
    float scale = img.renderScale_;
    if (scale > 10) {
      scale = 10;
    }
    if (scale < 0.01f) {
      scale = 0.01f;
    }
    std::string childName = "Image" + std::to_string(i);
    ImGui::BeginChild(childName.c_str(), ImVec2(0,0),true);
    ImGui::Image((void*)(intptr_t)img._texId,
                 ImVec2(float(img._drawWidth)* scale, float(img._drawHeight)*scale));
    ImGui::EndChild();
  }
}

void UILib::DrawFloatingTexts() {
  ImGui::BeginChild("Image0");
  ImDrawList* drawList = ImGui::GetForegroundDrawList();
  ImVec2 pos = ImGui::GetWindowPos();
  std::lock_guard<std::mutex> lock(floatingTextMutex_);
  for (size_t i = 0; i < floatingTexts_.size(); i++) {
    const auto& t = floatingTexts_[i];
    if (!t.draw_) {
      continue;
    }
    const char* textBegin = t.text_.c_str();
    const char* textEnd = textBegin + t.text_.size();
    drawList->AddText(ImVec2(pos[0] + t.pos_[0], pos[1] + t.pos_[1]),
                      t.color_,t.text_.c_str());
  }
  ImGui::EndChild();
}

void Button::Draw(){
  std::string bString = GetUniqueString();
  if (sameLine_) {
    ImGui::SameLine();
  }
  if (ImGui::Button(bString.c_str()) && _onClick) {
    _onClick();
  }
}

void Label::Draw() { ImGui::Text("%s", _text.c_str()); }

int UILib::LoadFonts(ImGuiIO&io) {
  std::string fontFile = _fontsDir + "/AvenirNextLTPro-Regular.otf";
  std::filesystem::path p(fontFile);
  if (!std::filesystem::exists(p)) {
    std::cout << "could not find " << fontFile << "\n";
    return -1;
  }
  ImFont* font = io.Fonts->AddFontFromFileTTF(fontFile.c_str(), 16.0f);    
  if (font == nullptr) {
    std::cout << "failed to load font " << fontFile << "\n";
    return -1;
  }
  return 0;
}

std::string loadTxtFile(std::string filename) {
  std::ifstream t(filename);
  if (!t.good()) {
    std::cout << "Cannot open " << filename << "\n";
    return "";
  }
  std::string str;

  t.seekg(0, std::ios::end);
  str.reserve(t.tellg());
  t.seekg(0, std::ios::beg);

  str.assign((std::istreambuf_iterator<char>(t)),
             std::istreambuf_iterator<char>());
  return str;
}

int UILib::LoadShaders() {
  std::string vs = loadTxtFile(_shaderDir + "/vs-no-tex.txt");
  std::string fs = loadTxtFile(_shaderDir + "/fs-no-tex.txt");
  if (vs.size() == 0 || fs.size() == 0) {
    std::cout << "could not locate shader file " << vs << "\n";
    return -1;
  }
  int ret = _glRender.Init(vs, fs);
  if (ret < 0) {
    std::cout << "failed to compile shaders" << vs << " " << fs << "\n";
    return ret;
  }
  return 0;
}

void UILib::SetWindowSize(unsigned width, unsigned height) {
  _width = width;
  _height = height;
}

void UILib::UILoop() {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    return ;
  }
  const char* glsl_version = "#version 330";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  // Create window with graphics context
  _window = glfwCreateWindow(_width, _height, _title.c_str(), NULL, NULL);
  if (_window == NULL) {
    std::cout << "failed to create glfw window\n";
    return ;
  }
  glfwMakeContextCurrent(_window);
  int majorv, minorv;
  glGetIntegerv(GL_MAJOR_VERSION, &majorv);
  glGetIntegerv(GL_MINOR_VERSION, &minorv);
  std::cout << "opengl " << majorv << "." << minorv << "\n";
  unsigned uret = glewInit();
  if (uret > 0) {
    std::cout << "init glew error " << uret << "\n";
  } else {
    int iret = LoadShaders();
    if (iret < 0) {
      std::cout << "failed to load shader. will not draw 3D meshes\n";
    }
  }

  glfwSwapInterval(1);  // Enable vsync
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  //io.ConfigWindowsMoveFromTitleBarOnly = true;
  (void)io;
  LoadFonts(io);
  ImGui::StyleColorsLight();
  io.ConfigWindowsMoveFromTitleBarOnly = true;
  ImGui_ImplGlfw_InitForOpenGL(_window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
  const unsigned RENDER_INTERVAL_MS = 10;
  while (!glfwWindowShouldClose(_window) && _running) {
    // Poll and handle events (inputs, window resize, etc.)
    // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to
    // tell if dear imgui wants to use your inputs.
    // - When io.WantCaptureMouse is true, do not dispatch mouse input data to
    // your main application, or clear/overwrite your copy of the mouse data.
    // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input
    // data to your main application, or clear/overwrite your copy of the
    // keyboard data. Generally you may always pass all inputs to dear imgui,
    // and hide them from your application based on those two flags.
    glfwPollEvents();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(0);
    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.8f, 0.8f, 0.8f, 0.5f)); 

    ImGui::SetNextWindowPos(ImVec2(20, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 800), ImGuiCond_FirstUseEver);

    if (_showGL) {
      _glRender.Render();
      ImGui::Begin("GL view", 0, ImGuiWindowFlags_NoBringToFrontOnFocus);
      ImGui::Image((ImTextureID)(size_t(_glRender.TexId())),
                   ImVec2(float(_glRender.Width()), float(_glRender.Height())));
      ImGui::End();
    }

    //image viewer
    ImGui::SetNextWindowPos(ImVec2(_initImagePosX, _initImagePosY),
                            ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags image_window_flags = 0;
    image_window_flags |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;
    image_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    std::string imageWinTitle;
    
    {
      std::lock_guard<std::mutex> lock(_widgetsLock);
      imageWinTitle = imageWindowTitle_;
    }
    ImGui::Begin(imageWinTitle.c_str(), 0, image_window_flags);
    ImVec2 pos = ImGui::GetWindowPos();
    if (pos.x < 0) {
      pos.x = 0;
    }
    if (pos.y < 0) {
      pos.y = 0;
    }
    ImGui::SetWindowPos(pos);
    //no padding
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    {
      std::lock_guard<std::mutex> lock(_imagesLock);
      DrawImages();
    }
    DrawFloatingTexts();
    ImGui::PopStyleVar();
    ImGui::End();
    //menu and buttons
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(100, 40), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags control_window_flags = 0;
    control_window_flags |= ImGuiWindowFlags_MenuBar;
    ImGui::Begin("Controls", nullptr, control_window_flags);   
    pos = ImGui::GetWindowPos();
    if (pos.x < 0) {
      pos.x= 0;
    }
    if (pos.y < 0) {
      pos.y = 0;    
    }
    ImGui::SetWindowPos(pos);
    ImGui::PopStyleColor();
    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open..", "Ctrl+O")) {
          _fileBrowser->Open();
        }
        if (ImGui::MenuItem("Save", "Ctrl+S")) { 
        }
        if (ImGui::MenuItem("Close", "Ctrl+W")) {
          
        }
        ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }
    for (size_t i = 0; i < uiWidgets_.size();i++) {
      uiWidgets_[i]->Draw();
    }
    _fileBrowser->Display();    
    ImGui::End();
    // Rendering

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(_clearColor[0], _clearColor[1], _clearColor[2],
                 _clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(_window);
    std::this_thread::sleep_for(std::chrono::milliseconds(RENDER_INTERVAL_MS));
  }
  _running = false;
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(_window);
  glfwTerminate();
}

void UILib::Run() {
  _running = true;
  std::function<void()> loopFun = std::bind(&UILib::UILoop, this);
  _uiThread = std::thread(&UILib::UILoop, this);
}

void UILib::Shutdown() {
  _running = false;
  if (_uiThread.joinable()) {
    _uiThread.join();
  }
}

std::string Button::GetUniqueString()const { return _text + "##" + std::to_string(_id); }

void CheckBox::Draw() { ImGui::Checkbox(label_.c_str(),&b_); }

void PlotWidget::Draw() {
  std::lock_guard<std::mutex> lock(mtx_);
 
  ImGui::PlotLines("", line_.data(), int(line_.size()), 0, title_.c_str(), min_, max_,
                   ImVec2(float(width_), float(height_)));
}
