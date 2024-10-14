#pragma once
#include <stdint.h>
#include <thread>
#include "Array2D.h"
#include "UIConf.h"
#include "UILib.h"
// draw stuff on an image
class CanvasApp {
 public:
  enum class State { IDLE, RUN, ONE_STEP };
  virtual ~CanvasApp();
  void Init(UILib* ui);
  void Log(const std::string& str) const;
  void RefreshSim();
  void RefreshUI();
  void Step();
  void SnapPic();
  std::function<void(const std::string&)> LogCb;

  int _drawRow = 0;
  uint64_t _num;
  int _imageId = -1;
  int _statusLabel = -1;
  bool _imageStale = false;

  Array2D4b _canvas;
  UILib* _ui;
  std::shared_ptr<InputInt> _resInput;
  int _numLabelId = -1;
  int _numStepsId = -1;
  UIConf conf;

  State _simState = State::IDLE;
  int _remainingSteps = 0;
  int _totalSteps = 0;
  std::thread _simThread;
  bool _snap = false;
};
