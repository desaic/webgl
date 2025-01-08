#include "PackApp.hpp"
#include "ImageUtils.h"
#include "StringUtil.h"
#include "imgui.h"
#include "MakeHelix.h"
#include <algorithm>

void PackApp::Init(UILib* ui) {
  _conf.Load(_conf._confFile);
  _ui = ui;
  Init3DScene(_ui);
  _ui->SetShowImage(false);

  _ui->AddButton("Container", [&]() {
    auto meshOpenCb =
        std::bind(&PackApp::QueueLoadContainer, this, std::placeholders::_1);
    _ui->SetFileOpenCb(meshOpenCb);
    bool openMulti = false;
    _ui->ShowFileOpen(openMulti, _conf.workingDir);
  });

  _ui->SetChangeDirCb(
      std::bind(&PackApp::OnChangeDir, this, std::placeholders::_1));

  _outDirWidget = std::make_shared<InputText>("out dir", _conf.outDir);
  _ui->AddWidget(_outDirWidget);
  
}

void PackApp::Init3DScene(UILib*ui) {
  
  _floor = std::make_shared<TrigMesh>();
  *_floor =
      MakePlane(Vec3f(-200, -0.1, -200), Vec3f(200, -0.1, 200), Vec3f(0, 1, 0));
  for (size_t i = 0; i < _floor->uv.size(); i++) {
    _floor->uv[i] *= 5;
  }
  Array2D8u checker;
  MakeCheckerPatternRGBA(checker);
  _floorInst = _ui->AddMeshAndInstance(_floor);
  _ui->SetInstTex(_floorInst, checker, 4);
  GLRender* gl = ui->GetGLRender();
  gl->SetDefaultCameraView(Vec3f(0, 20, -40), Vec3f(0));
  gl->SetDefaultCameraZ(0.2, 200);
  gl->SetPanSpeed(0.05);
  GLLightArray* lights = gl->GetLights();
  gl->SetZoomSpeed(2);
  lights->SetLightPos(0, 0, 20, 20);
  lights->SetLightPos(1, 0, 20, -20);
  lights->SetLightColor(0, 1,1,1);
  lights->SetLightColor(1, 0.8, 0.8, 0.8);
  lights->SetLightPos(2, 0, 100, 100);
  lights->SetLightColor(2, 0.8, 0.7, 0.6);
}

void PackApp::OnChangeDir(std::string dir) {
  _conf.workingDir = dir;
  _confChanged = true;
}

static int LoadMeshFile(const std::string& path, TrigMesh& mesh) {
  std::string suffix = get_suffix(path);
  std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                        std::tolower);
  if(suffix=="obj"){
    mesh.LoadObj(path);
    return 0;
  } else if (suffix == "stl") {
    mesh.LoadStl(path);
    return 0;
  }
  return -1;
}

void PackApp::OpenFiles(const std::vector<std::string>& files) {
  if (files.size() == 0) {
    return;
  }
  
}

void PackApp::QueueLoadContainer(const std::string& file) {
  QueueCommand(std::make_shared<LoadContainerCmd>(this, file));
}

void PackApp::LoadContainer(const std::string& file) {
  _container = std::make_shared<TrigMesh>();
  int ret = LoadMeshFile(file, *_container);
  if (ret == 0) {
    _containerInst = _ui->AddMeshAndInstance(_container);
  }
}

void PackApp::Refresh() {
  ExeCommands();
  if (_outDirWidget->_entered) {
    _conf.outDir = _outDirWidget->GetString();
    _confChanged = true;
  }
  if (_confChanged) {
    _conf.Save();
  }
} 

void PackApp::ExeCommands() {
  while (!_commandQueue.empty()) {
    CmdPtr cmdPtr;
    bool has = _commandQueue.try_pop(cmdPtr);
    if (cmdPtr) {
      cmdPtr->Run();
    }
  }
}

void PackApp::QueueCommand(CmdPtr cmd) {
  if (_commandQueue.size() < MAX_COMMAND_QUEUE_SIZE) {
    _commandQueue.push(cmd);
  }
}

void LoadContainerCmd::Run() { app->LoadContainer(_filename); }