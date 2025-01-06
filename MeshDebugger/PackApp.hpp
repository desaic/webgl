#pragma once
#include "TrigMesh.h"
#include "UIConf.h"
#include "UILib.h"
#include "threadsafe_queue.h"
#include "MakeHelix.h"

class PackApp;

struct PackCommand {
  std::string _name = "";
  PackCommand(const std::string& name) : _name(name) {}
  virtual const std::string& GetName() const { return _name; }
  virtual ~PackCommand() {}
  virtual void Run(PackApp& app) {}
};

struct OpenCommand : public PackCommand {
  OpenCommand() : PackCommand("open") {}
  std::vector<std::string> _filenames;
  virtual void Run(PackApp& app) override;
};

typedef std::shared_ptr<PackCommand> CadCmdPtr;

class cad_app {
 public:
  void Init(UILib* ui);
  void Refresh();

  void ExeCommands();

  void OpenFiles(const std::vector<std::string>& files);
  void OnChangeDir(std::string dir);

  void QueueCommand(CadCmdPtr cmd);
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
  threadsafe_queue<CadCmdPtr> _commandQueue;

  std::shared_ptr<InputText> _outDirWidget;
};
