#include "HeadApp.hpp"
#include "StringUtil.h"
#include "Stopwatch.h"
#include "BBox.h"
#include "MeshUtil.h"
#include "cpu_voxelizer.h"
#include "RaycastConf.h"
#include "VoxIO.h"
#include "ZRaycast.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>

static void OnChangeDir(std::string dir, UIConf& conf) {
  conf.workingDir = dir;
  conf.Save();
}

static void LogToUI(const std::string& str, UILib& ui, int statusLabel) {
  ui.SetLabelText(statusLabel, str);
}

void HeadApp::Init(UILib* ui) {
  _ui = ui;
  conf.Load(conf._confFile);
  _ui->AddButton("Open Meshes", [&] { this->OpenMeshes(*_ui, conf); });
  _ui->SetChangeDirCb([&](const std::string& dir) { OnChangeDir(dir, conf); });
  _statusLabel = ui->AddLabel("status");
  LogCb =
      std::bind(LogToUI, std::placeholders::_1, std::ref(*_ui), _statusLabel);
  _outPrefixId =
      _ui->AddWidget(std::make_shared<InputText>("output file", "grid.txt"));
  _ui->SetShowImage(false);

  Init3DScene(_ui);
}

void HeadApp::Init3DScene(UILib* ui) {
  _floor = std::make_shared<TrigMesh>();
  *_floor =
      MakePlane(Vec3f(-500, -1, -500), Vec3f(500, -1, 500), Vec3f(0, 1, 0));

  _floorInst = _ui->AddMeshAndInstance(_floor);
  
  GLRender* gl = ui->GetGLRender();
  _floorColor.Allocate(12, 4);
  _floorColor.Fill(230);
  int floorId = gl->GetInstance(_floorInst).bufId;
  gl->SetMeshTexture(floorId, _floorColor, 3);

  gl->SetDefaultCameraView(Vec3f(0, 200, -400), Vec3f(0));
  gl->SetDefaultCameraZ(2, 2000);
  gl->SetPanSpeed(0.5);  
  GLLightArray* lights = gl->GetLights();
  gl->SetZoomSpeed(20);
  lights->SetLightPos(0, 300, 1000, 300);
  lights->SetLightPos(1, -300, 1000, -300);
  lights->SetLightColor(0, 1, 1, 1);
  lights->SetLightColor(1, 0.8, 0.8, 0.8);
}

//info for voxelizing
//multi-material interface connector designs
void HeadApp::Log(const std::string& str) const {
  if (LogCb) {
    LogCb(str);
  } else {
    std::cout << str << "\n";
  }
}

BBox GuessAndScaleMeshes(std::vector<std::shared_ptr<TrigMesh>>& meshes,
                         float approxSize) {
  BBox bbox;
  for (size_t i = 0; i < meshes.size(); i++) {
    UpdateBBox(meshes[i]->v, bbox);
  }
  Vec3f boxSize = bbox.vmax - bbox.vmin;
  float boxLen = std::max(boxSize[0], std::max(boxSize[1],boxSize[2]));
  const unsigned NUM_GUESS = 5;
  const float POTENTIAL_SCALES[NUM_GUESS] = {1, 10, 100, 1000, 25.4};
  float minDiff = std::abs(approxSize - boxLen);
  unsigned scaleIndex = 0;
  for (unsigned i = 0; i < NUM_GUESS; i++) {
    float diff = std::abs(float(POTENTIAL_SCALES[i] * boxLen - approxSize));
    if (diff < minDiff) {
      minDiff = diff;
      scaleIndex = i;
    }
  }
  float scale = POTENTIAL_SCALES[scaleIndex];
  if (std::abs(scale-1) >1e-4) {
    std::cout << "scale meshes by " << scale << "\n";
  }
  for (unsigned i = 0; i < meshes.size(); i++) {
    meshes[i]->scale(scale);
  }
  bbox.vmin *= scale;
  bbox.vmax *= scale;
  return bbox;
}

void HeadApp::LoadMeshes() {
  if (filenames.size() == 0) {
    return;
  }
  _meshes.clear();
  _meshes.resize(filenames.size());
  Log("load meshes");
  for (size_t i = 0; i < filenames.size();i++) {
    const auto& file = filenames[i]; 
    std::string ext = get_suffix(file);
    std::cout << ext << "\n";
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (ext.size() != 3) {
      continue;
    }
    _meshes[i] = std::make_shared<TrigMesh>();
    if (ext == "obj") {
      _meshes[i]->LoadObj(file);
    } else if (ext == "stl") {
      _meshes[i]->LoadStl(file);
    }
  }
  std::filesystem::path filePath(filenames[0]);
  dir = filePath.parent_path().string();
  _fileLoaded = true;

  BBox box = GuessAndScaleMeshes(_meshes, ApproxSize);
  auto & floorInst = _ui->GetGLRender()->GetInstance(_floorInst);
  floorInst.matrix(1, 3) += box.vmin[1];

  _instanceIds.resize(_meshes.size());
  for (size_t i = 0; i < _meshes.size(); i++) {
    _instanceIds[i] = _ui->AddMeshAndInstance(_meshes[i]);
  }
  Log("load meshes done");
}

static bool FloatEq(float a, float b, float eps) { return std::abs(a - b) < eps; }

/// @brief 
/// @param widgetId 
/// @param ui 
/// @param confVal 
/// @param inputScale 
/// @return 0 if no change. >0 if value changed. 
static int CheckFloatInput(int widgetId, UILib* ui, float& confVal,
                           float inputScale,
  float minVal) {
  std::shared_ptr<InputInt> input =
      std::dynamic_pointer_cast<InputInt>(ui->GetWidget(widgetId));
  float uiVal = confVal;
  if (input && input->_entered) {
    uiVal = float(input->_value * inputScale);
    input->_entered = false;
  }
  const float EPS = 5e-5f;
  uiVal = std::max(minVal, uiVal);
  if (!FloatEq(uiVal, confVal, EPS)) {
    confVal = uiVal;
    return 1;
  }
  return 0;
}

void HeadApp::Refresh() {
  //update configs
  int confChanged = 0;
  const float EPS = 5e-5f;
  const float umToMM=0.001f;

  if (_outPrefixId >= 0) {
    std::shared_ptr<InputText> input =
        std::dynamic_pointer_cast<InputText>(_ui->GetWidget(_outPrefixId));
    if (input && input->_entered) {
      input->_entered = false;
    }
    if (conf.outputFile != input->GetString()) {
      conf.outputFile = input->GetString();
      confChanged = true;
    }
  }
  if (confChanged) {
    conf.Save();
  }

  if (_hasFileNames && !_fileLoaded) {
    LoadMeshes();
    _processed = false;
  }
  if (_fileLoaded && !_processed) {
    
  }
}

void HeadApp::OnOpenMeshes(const std::vector<std::string>& files) {
  filenames = files;
  _fileLoaded = false;
  _hasFileNames = true;
}

void HeadApp::OpenMeshes(UILib& ui, const UIConf& conf) {
  ui.SetMultipleFilesOpenCb(
      std::bind(&HeadApp::OnOpenMeshes, this, std::placeholders::_1));
  ui.ShowFileOpen(true, conf.workingDir);
}
