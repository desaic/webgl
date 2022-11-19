#include "UILib.h"

#include <functional>
#include <iostream>
#include "imgui.h"
#include <Windows.h>
#include <GLFW/glfw3.h>  // Will drag system OpenGL headers

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imfilebrowser.h"

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

int UILib::AddButton(const std::string& text,
                     const std::function<void()>& onClick) {
  std::lock_guard<std::mutex> lock(_buttonsLock);
  int id = int(_buttons.size());
  _buttons.push_back(Button());
  Button& bt = _buttons.back();
  bt._id = id;
  bt._text = text;
  bt._onClick = onClick;
  return id;
}

int UILib::AddLabel(const std::string& text) {
  std::lock_guard<std::mutex> lock(_labelsLock);
  int id = int(_labels.size()); 
  _labels.push_back(Label());
  _labels.back()._text = text;
  return id;
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
                   d._width == size[0] && d._numChan == inputChan);
  d._drawHeight = size[1];
  d._drawWidth = size[0];  
  d._numChan = inputChan;
  d._width = size[0]*4;
  d._height = size[1];
  Vec4bTo8u(image.GetData(), d._buf);
  d._needUpdate = true;
  return 0;
}

void UILib::DrawImages() {
  for (size_t i = 0; i < _images.size(); i++) {
    GLTex& img = _images[i];
    if (img._needUpdate) {
      img.UpdateTextureData();
      img._needUpdate = false;
    }
    ImGui::Image((void*)(intptr_t)img._texId,
                 ImVec2(float(img._drawWidth), float(img._drawHeight)));
  }
}

void UILib::HandleButtons() {
  for (size_t i = 0; i < _buttons.size(); i++) {
    const Button& bt = _buttons[i];
    std::string bString = bt.GetUniqueString();
    if (ImGui::Button(bString.c_str()) && bt._onClick) {
      bt._onClick();
    }
  }
}

void UILib::DrawLabels() {
  for (size_t i = 0; i < _labels.size(); i++) {
    ImGui::Text("%s", _labels[i]._text.c_str());
  }
}

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

void UILib::UILoop() {
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    return ;
  }

  const char* glsl_version = "#version 330";
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  // Create window with graphics context
  _window = glfwCreateWindow(_width, _height, "ImGui example", NULL, NULL);
  if (_window == NULL) {
    std::cout << "failed to create glfw window\n";
    return ;
  }
  glfwMakeContextCurrent(_window);

  std::string glInfo = glRender.GLInfo();
  std::cout << "open gl info: " << glInfo << "\n";

  glfwSwapInterval(1);  // Enable vsync
  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  //io.ConfigWindowsMoveFromTitleBarOnly = true;
  (void)io;
  LoadFonts(io);
  ImGui::StyleColorsLight();

  ImGui_ImplGlfw_InitForOpenGL(_window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);
  while (!glfwWindowShouldClose(_window)) {
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

    // Start the Dear ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    //image viewer
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags image_window_flags = 0;
    image_window_flags |= ImGuiWindowFlags_NoBackground;
    image_window_flags |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;
    image_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::Begin("Image", 0,image_window_flags);
    {
      std::lock_guard<std::mutex> lock(_imagesLock);
      DrawImages();
    }
    ImGui::End();

    //menu and buttons
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(0, 0), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags control_window_flags = 0;
    control_window_flags |= ImGuiWindowFlags_NoBackground;
    control_window_flags |= ImGuiWindowFlags_MenuBar;
    ImGui::Begin("Controls", nullptr, control_window_flags);   
    ImVec2 pos = ImGui::GetWindowPos();
    if (pos.x < 0) {
      pos.x= 0;
    }
    if (pos.y < 0) {
      pos.y = 0;    
    }
    ImGui::SetWindowPos(pos);
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
    {
      std::lock_guard<std::mutex> lock(_buttonsLock);
      HandleButtons();
    }
    {
      std::lock_guard<std::mutex> lock(_labelsLock);
      DrawLabels();
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
  }
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(_window);
  glfwTerminate();
}

void UILib::Run() {
  std::function<void()> loopFun = std::bind(&UILib::UILoop, this);
  _uiThread = std::thread(&UILib::UILoop, this);
}

void UILib::Shutdown() {
  if (_uiThread.joinable()) {
    _uiThread.join();
  }
}

std::string Button::GetUniqueString()const { return _text + "##" + std::to_string(_id); }
