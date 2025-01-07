#include "PackApp.hpp"
#include "ImageUtils.h"
#include "StringUtil.h"
#include "imgui.h"
#include "MakeHelix.h"
#include <algorithm>

void PackApp::Init(UILib* ui) {
  _conf.Load(_conf._confFile);
  _ui = ui;
  _floor = std::make_shared<TrigMesh>();
  *_floor =
      MakePlane(Vec3f(-200, -0.1, -200), Vec3f(200, -0.1, 200), Vec3f(0, 1, 0));
  for (size_t i = 0; i < _floor->uv.size(); i++) {
    _floor->uv[i] *= 5;
  }
  Array2D8u checker;
  MakeCheckerPatternRGBA(checker);
  _floorMeshId = _ui->AddMesh(_floor);
  _ui->SetMeshTex(_floorMeshId, checker, 4);
  GLRender* gl = ui->GetGLRender();
  GLLightArray* lights = gl->GetLights();
  lights->SetLightPos(0, 250, 100, 250);
  lights->SetLightPos(1, 500, 100, 250);
  lights->SetLightColor(0, 0.8, 0.7, 0.6);
  lights->SetLightColor(1, 0.8, 0.8, 0.8);
  lights->SetLightPos(2, 0, 100, 100);
  lights->SetLightColor(2, 0.8, 0.7, 0.6);
  _ui->SetShowImage(false);

  _ui->AddButton("Open", [&]() {
    auto meshOpenCb =
        std::bind(&PackApp::QueueOpenFiles, this, std::placeholders::_1);
    _ui->SetMultipleFilesOpenCb(meshOpenCb);
    _ui->ShowFileOpen(true, _conf.workingDir);
  });

  _ui->SetChangeDirCb(
      std::bind(&PackApp::OnChangeDir, this, std::placeholders::_1));

  _outDirWidget = std::make_shared<InputText>("out dir", _conf.outDir);
  _ui->AddWidget(_outDirWidget);
  
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

void PackApp::QueueOpenFiles(const std::vector<std::string>& files) {
  std::shared_ptr<OpenCommand> cmd = std::make_shared<OpenCommand>();
  cmd->_filenames = files;
  QueueCommand(cmd);
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

void OpenCommand::Run() { app->OpenFiles(_filenames); }