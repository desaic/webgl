#include "cad_app.h"
#include "ImageUtils.h"
#include "StringUtil.h"
#include <algorithm>
void cad_app::Init(UILib* ui) {
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

  _ui->SetShowImage(false);

  _ui->AddButton("Open", [&]() {
    auto meshOpenCb =
        std::bind(&cad_app::QueueOpenFiles, this, std::placeholders::_1);
    _ui->SetMultipleFilesOpenCb(meshOpenCb);
    _ui->ShowFileOpen(true, _conf.workingDir);
  });

  _ui->SetChangeDirCb(std::bind(&cad_app::OnChangeDir, this, std::placeholders::_1));
}

void cad_app::OnChangeDir(std::string dir) {
  _conf.workingDir = dir;
  _confChanged = true;
}

int LoadMeshFile(const std::string& path, TrigMesh& mesh) {
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

void cad_app::OpenFiles(const std::vector<std::string>& files) {
  if (files.size() == 0) {
    return;
  }
  Part p;
  p.id = _parts.size();
  p.name = get_file_name(files[0]);
  for (size_t i = 0; i < files.size(); i++) {
    std::cout << files[i] << "\n";
    std::shared_ptr<TrigMesh> meshPtr = std::make_shared<TrigMesh>();
    int ret = LoadMeshFile(files[i], *meshPtr);
    if (ret < 0) {
      continue;
    }
    int id = _ui->AddMesh(meshPtr);
    p._meshIds.push_back(id);
    p.meshes.push_back(meshPtr);
  }
  _parts.push_back(p);
}

void cad_app::QueueOpenFiles(const std::vector<std::string>& files) {
  std::shared_ptr<OpenCommand> cmd = std::make_shared<OpenCommand>();
  cmd->_filenames = files;
  QueueCommand(cmd);
}

void cad_app::Refresh() {
  ExeCommands();
  if (_confChanged) {
    _conf.Save();
  }
} 

void cad_app::ExeCommands() {
  while (!_commandQueue.empty()) {
    std::shared_ptr<CadCommand> cmdPtr;
    bool has = _commandQueue.try_pop(cmdPtr);
    if (cmdPtr) {
      cmdPtr->Run(*this);
    }
  }
}

void cad_app::QueueCommand(CadCmdPtr cmd) {
  if (_commandQueue.size() < MAX_COMMAND_QUEUE_SIZE) {
    _commandQueue.push(cmd);
  }
}

void OpenCommand::Run(cad_app& app) { app.OpenFiles(_filenames); }