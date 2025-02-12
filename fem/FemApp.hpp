#pragma once
class UILib;
#include "FESim.hpp"
#include "ElementMesh.h"
#include "FETrigMesh.hpp"
#include <mutex>
#include <string>

class FemApp {
 public:
  static const float MToMM;

  FemApp(UILib* ui);

  std::string GetInputString(int widgetId) const;

  void InitExternalForce();

  void LoadElementMesh(const std::string& file);

  void UpdateRenderMesh();

  void SaveX(const ElementMesh& em, const std::string& outFile);

  void Refresh();

 private:
  UILib* _ui = nullptr;
  ElementMesh _em;
  FETrigMesh _renderMesh;
  int _hexInputId = -1;
  int _xInputId = -1;
  int _meshId = -1;
  int _wireframeId = -1;
  float _drawingScale = MToMM;
  std::shared_ptr<TrigMesh> floor;
  std::shared_ptr<TrigMesh> _floor;
  int _floorInst = -1;

  FESim _sim;
  SimState _simState;
  std::mutex _refresh_mutex;
  bool _save_x = false;
  bool _runSim = false;
  // number of simulation steps to run.
  // negative number to run forever
  int _remainingSteps = -1;
  int _stepCount = 0;

  float _prevE = 0;
  int _moveCount = 0;
};
