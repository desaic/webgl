#include "CanvasApp.h"
#include "StringUtil.h"
#include "ImageIO.h"
#include "Timer.h"
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
  _ui->SetImageData(_imageId, _canvas); 
  _num = NextNum(_num);
  if (_num == 3562942561397226080ull / 32) {
    std::cout << "hooray!\n";
  }
}

void CanvasApp::SnapPic() { SavePngColor("./snap.png", _canvas); }

void CanvasApp::Refresh() {
  //update configs
  bool confChanged = false;
  const float EPS = 5e-5f;
  const float umToMM=0.001f;

  if (confChanged) {
    conf.Save();
  }
  switch (_state) {
    case State::RUN:
      Step();
      break;
    case State::ONE_STEP:
      Step();
      _state = State::IDLE;
      break;
    default:
      break;
  }
  _ui->SetLabelText(_numLabelId, std::to_string(_num));
  if (_snap) {
    Array2D4b out = _canvas;
    SavePngColor("canvas.png", _canvas);
    _snap = false;
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
  _ui->AddButton("Run", [&] { this->_state = State::RUN; });
  _ui->AddButton("One Step", [&] { this->_state = State::ONE_STEP; });
  _ui->AddButton("Stop", [&] { this->_state = State::IDLE; });
  _ui->AddButton("Snap Pic", [&] { this->_snap = true; });
  _numLabelId = _ui->AddLabel("num: ");
  _ui->SetChangeDirCb(
      [&](const std::string& dir) { OnChangeDir(dir, conf); });
  _statusLabel = ui->AddLabel("status");
  LogCb =
      std::bind(LogToUI, std::placeholders::_1, std::ref(*_ui), _statusLabel);
}
