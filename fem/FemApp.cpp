#pragma once
#include "FemApp.hpp"
#include "ElementMeshUtil.h"
#include "ImageUtils.h"
#include "UILib.h"
#include <fstream>
const float FemApp::MToMM = 1000;
FemApp::FemApp(UILib* ui) : _ui(ui) {
  // EmbedMesh();
  floor = std::make_shared<TrigMesh>();
  *floor =
      MakePlane(Vec3f(-0, -0.1, -0), Vec3f(500, -0.1, 500), Vec3f(0, 1, 0));
  for (size_t i = 0; i < floor->uv.size(); i++) {
    floor->uv[i] *= 5;
  }
  GLRender* gl = ui->GetGLRender();
  gl->SetDefaultCameraView(Vec3f(250, 100, 500), Vec3f(200, 0, 250));
  GLLightArray* lights = gl->GetLights();
  lights->SetLightPos(0, 250, 100, 250);
  lights->SetLightPos(1, 500, 100, 250);
  lights->SetLightColor(0, 0.8, 0.7, 0.6);
  lights->SetLightColor(1, 0.8, 0.8, 0.8);
  lights->SetLightPos(2, 0, 100, 100);
  lights->SetLightColor(2, 0.8, 0.7, 0.6);
  Array2D8u checker;
  MakeCheckerPatternRGBA(checker);
  _floorMeshId = _ui->AddMesh(floor);
  _ui->SetMeshTex(_floorMeshId, checker, 4);
  std::string voxFile = "F:/dolphin/meshes/topopt/beam2mm_cut.txt";
  _hexInputId =
      _ui->AddWidget(std::make_shared<InputText>("mesh file", voxFile));
  _ui->AddButton("Load Hex mesh", [&] {
    std::string file = GetInputString(_hexInputId);
    LoadElementMesh(file);
  });
  _ui->AddButton("Save x", [&] { _save_x = true; });
  _ui->AddButton("Run sim", [&] {
    _runSim = true;
    _remainingSteps = -1;
  });
  _ui->AddButton(
      "Step sim",
      [&] {
        _runSim = true;
        _remainingSteps = 1;
      },
      true);
  _ui->AddButton("Stop sim", [&] {
    _runSim = false;
  });
  _xInputId =
      _ui->AddWidget(std::make_shared<InputText>("x file", "F:/dump/x_in.txt"));
  _ui->AddButton("Load x", [&] {
    std::string file = GetInputString(_xInputId);
    LoadX(_em, file);
  });
  _wireframeId = _ui->AddCheckBox("showWireFrame", true);
}

std::string FemApp::GetInputString(int widgetId) const {
  std::shared_ptr<InputText> input =
      std::dynamic_pointer_cast<InputText>(_ui->GetWidget(widgetId));
  std::string file = input->GetString();
  return file;
}

void FemApp::InitExternalForce() {
  _em.fe = std::vector<Vec3f>(_em.X.size());
  _em.fixedDOF = std::vector<bool>(_em.X.size() * 3, false);
  _em.lb.resize(_em.X.size(), Vec3f(-1000, -1000, -1000));
  // PullRight(Vec3f(0,-0.001,  0), 0.01, _em);
  // PullLeft(Vec3f(0,-0.001, 0), 0.01, _em);
  FixLeft(0.005, _em);
  FixRight(0.005, _em);
  MoveRightEnd(0.005, -0.005, _em);
  // PullMiddle(Vec3f(0, 0.002, 0), 0.03, _em);
  // FixFloorLeft(0.4, _em);
  //_em.lb.resize(_em.X.size(), Vec3f(-1000, -1, -1000));
  // SetFloorForRightSide(0.0055, 0.4, _em);
  // FixMiddle(0.03, _em);
  // AddGravity(Vec3f(0, -0.0005, 0),_em);
}

void FemApp::LoadElementMesh(const std::string& file) {
  std::lock_guard<std::mutex> lock(_refresh_mutex);
  _em.LoadTxt(file);
  UpdateRenderMesh();
  InitExternalForce();
  _sim.InitCG(_em, _simState);
}

void FemApp::UpdateRenderMesh() {
  _renderMesh.UpdateMesh(&_em, _drawingScale);
  if (_meshId < 0) {
    _meshId = _ui->AddMesh(_renderMesh._mesh);
  }
  _ui->SetMeshNeedsUpdate(_meshId);
}

void FemApp::SaveX(const ElementMesh& em, const std::string& outFile) {
  std::ofstream out(outFile);
  out << em.x.size() << "\n";
  for (size_t i = 0; i < em.x.size(); i++) {
    out << em.x[i][0] << " " << em.x[i][1] << " " << em.x[i][2] << "\n";
  }
}

void FemApp::Refresh() {
  std::lock_guard<std::mutex> lock(_refresh_mutex);
  bool wire = _ui->GetCheckBoxVal(_wireframeId);
  _renderMesh.UpdateWireFrame(wire);
  if (_runSim && (_remainingSteps != 0)) {
    if (_remainingSteps > 0) {
      _remainingSteps--;
    }
    _sim.StepCG(_em, _simState);
    _stepCount++;
    bool converge = false;
    if (!_em.e.empty()) {
      float E = _em.GetPotentialEnergy();
      converge = std::abs(E - _prevE) < 0.01f;
      _prevE = E;
    }

    if (converge && !_em.e.empty() && _moveCount < 20) {
      auto f = _em.GetForce();
      float totalForce = 0;
      for (size_t i = 0; i < _em.X.size(); i++) {
        if (_em.X[i][0] >= 0.205) {
          totalForce += f[i][0];
        }
      }
      std::cout << _moveCount << " " << totalForce << " "
                << "\n";
      MoveRightEnd(0.005, -0.005, _em);
      _moveCount++;
    }
    if (_moveCount >= 20) {
    }
    _renderMesh.UpdatePosition();
  }
  if (_renderMesh._updated) {
    _ui->SetMeshNeedsUpdate(_meshId);
    _renderMesh._updated = false;
  }
  if (_save_x) {
    SaveX(_em, "F:/dump/x_out.txt");
    _save_x = false;
  }
}
