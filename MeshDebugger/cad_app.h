#pragma once
#include "TrigMesh.h"
#include "UILib.h"

class cad_app {
 public:
  void Init(UILib* ui);
  void Refresh();
 private:
  UILib* _ui;
  std::shared_ptr<TrigMesh> _floor;
  int _floorMeshId = -1;
};