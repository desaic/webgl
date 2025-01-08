#pragma once
#include "Command.hpp"
#include "TrigMesh.h"
#include "UIConf.h"
#include "UILib.h"
#include "threadsafe_queue.h"
#include "MakeHelix.h"

class PackApp;

struct LoadContainerCmd : public Command {
  LoadContainerCmd() : Command("container") {}
  LoadContainerCmd(PackApp* a, const std::string& f)
      : Command("container"), app(a), _filename(f) {}
  virtual void Run() override;
  PackApp* app = nullptr;
  std::string _filename;
};

class PackApp {
 public:
  void Init(UILib* ui);
  void Refresh();

  void ExeCommands();

  void OpenFiles(const std::vector<std::string>& files);
  void OnChangeDir(std::string dir);

  void QueueCommand(CmdPtr cmd);

  void QueueLoadContainer(const std::string& file);
  void LoadContainer(const std::string& file);

 protected:
  UILib* _ui;
  std::shared_ptr<TrigMesh> _floor;
  std::shared_ptr<TrigMesh> _container;
  
  int _floorInst = -1;
  int _containerInst = -1;

  UIConf _conf;
  bool _confChanged = false;
  const static size_t MAX_COMMAND_QUEUE_SIZE = 10;
  threadsafe_queue<CmdPtr> _commandQueue;
  std::shared_ptr<InputText> _outDirWidget;

  void Init3DScene(UILib* ui);
};
