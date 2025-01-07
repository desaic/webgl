#pragma once
#include "Command.hpp"
#include "TrigMesh.h"
#include "UIConf.h"
#include "UILib.h"
#include "threadsafe_queue.h"
#include "MakeHelix.h"

struct Part {
  std::vector<std::shared_ptr<TrigMesh> > meshes;
  std::vector<int> _meshIds;
  std::string name;
  int id = 0;
};

class CadApp;

struct OpenCadMeshes : public Command {
  OpenCadMeshes() : Command("open") {}
  std::vector<std::string> _filenames;
  virtual void Run() override;
  CadApp* app = nullptr;
};

class CadApp {
 public:
  void Init(UILib* ui);
  void Refresh();

  void ExeCommands();

  void OpenFiles(const std::vector<std::string>& files);
  void OnChangeDir(std::string dir);

  void QueueCommand(CmdPtr cmd);
  void QueueOpenFiles(const std::vector<std::string>& files);

  void QueueHelix(const HelixSpec& spec);
  void AddHelix(const HelixSpec& spec);
 protected:
  UILib* _ui;
  std::shared_ptr<TrigMesh> _floor;
  int _floorMeshId = -1;
  UIConf _conf;
  bool _confChanged = false;
  std::vector<Part> _parts;
  const static size_t MAX_COMMAND_QUEUE_SIZE = 10;
  CmdQueue _commandQueue;

  std::shared_ptr<InputText> _outDirWidget;
};
