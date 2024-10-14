#include "CanvasApp.h"
#include "StringUtil.h"
#include "ImageIO.h"
#include "BBox.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <bit>
//info for voxelizing
//multi-material interface connector designs
void CanvasApp::Log(const std::string& str) const {
  if (LogCb) {
    LogCb(str);
  } else {
    std::cout << str << "\n";
  }
}

void DrawRow(uint64_t num, int row, Array2D4b & image) {
  Vec2u size = image.GetSize();
  if (row < 0|| row>=size[1]) {
    return;  
  }
  
  for (unsigned x = 0; x < size[0]; x++) {
    Vec4b color(200,200,0,255);
    if (num % 2 == 0) {
      color = Vec4b(0, 0, 200, 255);
    }
    num /= 2;
    image(x, row) = color;
  }
}

uint64_t pow3(int a) { uint64_t val = 1;
  for (int i = 0; i < a; i++) {
    val *= 3;
  }
  return val;
}

uint64_t NextNum(uint64_t num) {
  if (num < 10) {
    return num;
  }

  uint64_t n = num + 1;
  int a = std::_Countr_zero(n);
  n = n >> a;
  n = n *pow3(a);
  n = n - 1;
  int b = std::_Countr_zero(n);
  n = n >> b;
  return n;
}

void CanvasApp::Step() {
  DrawRow(_num, _drawRow, _canvas);
  _drawRow++;
  _drawRow %= _canvas.GetSize()[1];
  _num = NextNum(_num);
  _imageStale = true;
}

void CanvasApp::SnapPic() { SavePngColor("./snap.png", _canvas); }
void CanvasApp::RefreshSim() {  
  while (_ui->IsRunning()) {
    switch (_simState) {
      case State::RUN:
        if (_remainingSteps > 0) {
          Step();
          _remainingSteps--;
        }
        break;
      case State::ONE_STEP:
        Step();
        _simState = State::IDLE;
        break;
      default:
        break;
    }
  }
}

  void CanvasApp::RefreshUI() {
  //update configs
  bool confChanged = false;
  const float EPS = 5e-5f;
  const float umToMM=0.001f;

  if (confChanged) {
    conf.Save();
  }
  auto inputWidget = _ui->GetWidget(_numStepsId);
  if (inputWidget) {
    const InputInt* stepsInput = (const InputInt*)inputWidget.get();
    _totalSteps = stepsInput->_value;
  }
  _ui->SetLabelText(_numLabelId, std::to_string(_num));
  if (_snap) {
    Array2D4b out = _canvas;
    SavePngColor("canvas.png", _canvas);
    _snap = false;
  }
  if (_imageStale) {
    _ui->SetImageData(_imageId, _canvas);
    _imageStale = false;
  }
}

static void OnChangeDir(std::string dir, UIConf& conf) {
  conf.workingDir = dir;
  conf.Save();
}

static void LogToUI(const std::string& str, UILib& ui, int statusLabel) {
  ui.SetLabelText(statusLabel, str);
}

void CanvasApp::Init(UILib* ui) { 
  _ui = ui;
  _ui->SetShowImage(true);
  _num = 1410123943ull;
  _canvas.Allocate(800, 800);
  _canvas.Fill(Vec4b(127, 127, 0, 127));
  _imageId = _ui->AddImage();
  _ui->SetImageData(_imageId, _canvas);
  conf.Load(conf._confFile);
  _ui->AddButton("Run", [&] {
    this->_remainingSteps = _totalSteps;
    this->_simState = State::RUN;
  });
  _ui->AddButton("One Step", [&] { this->_simState = State::ONE_STEP; });
  _ui->AddButton("Stop", [&] { this->_simState = State::IDLE; });
  _ui->AddButton("Snap Pic", [&] { this->_snap = true; });
  std::shared_ptr<InputInt> numStepsInput = std::make_shared<InputInt>("#steps", 100);
  _numStepsId = _ui->AddWidget(numStepsInput);
  _numLabelId = _ui->AddLabel("num: ");
  _ui->SetChangeDirCb(
      [&](const std::string& dir) { OnChangeDir(dir, conf); });
  _statusLabel = ui->AddLabel("status");
  LogCb =
      std::bind(LogToUI, std::placeholders::_1, std::ref(*_ui), _statusLabel);
  _simThread = std::thread(&CanvasApp::RefreshSim, this);
}

CanvasApp::~CanvasApp() {
  if (_simThread.joinable()) {
    _simThread.join();
  }
}