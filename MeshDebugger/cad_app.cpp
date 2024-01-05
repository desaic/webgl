#include "cad_app.h"
#include "ImageUtils.h"

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

void cad_app::OpenFiles(const std::vector<std::string>& files) {
  Part p;
  for (size_t i = 0; i < files.size(); i++) {
    std::cout << files[i] << "\n";
    
  }
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