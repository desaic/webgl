#pragma once
#include "Command.hpp"
#include "TrigMesh.h"
#include "UIConf.h"
#include "UILib.h"
#include "threadsafe_queue.h"
#include "MakeHelix.h"

class PackApp;

struct OpenCommand : public Command {
  OpenCommand() : Command("open") {}
  std::vector<std::string> _filenames;
  virtual void Run() override;
  PackApp* app = nullptr;
};

class PackApp {
 public:
  void Init(UILib* ui);
  void Refresh();

  void ExeCommands();

  void OpenFiles(const std::vector<std::string>& files);
  void OnChangeDir(std::string dir);

  void QueueCommand(CmdPtr cmd);
  void QueueOpenFiles(const std::vector<std::string>& files);

 protected:
  UILib* _ui;
  std::shared_ptr<TrigMesh> _floor;
  int _floorMeshId = -1;
  UIConf _conf;
  bool _confChanged = false;
  const static size_t MAX_COMMAND_QUEUE_SIZE = 10;
  threadsafe_queue<CmdPtr> _commandQueue;

  std::shared_ptr<InputText> _outDirWidget;
};
