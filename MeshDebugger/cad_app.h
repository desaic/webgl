#pragma once
#include "TrigMesh.h"
#include "UIConf.h"
#include "UILib.h"
#include "threadsafe_queue.h"

struct Part {
  std::vector<std::shared_ptr<TrigMesh> > meshes;
  std::vector<int> _meshIds;
  std::string name;
  int id = 0;
};

class cad_app;

struct CadCommand {
  std::string _name = "";
  CadCommand(const std::string& name) : _name(name) {}
  virtual const std::string& GetName() const { return _name; }
  virtual ~CadCommand() {}
  virtual void Run(cad_app& app) {}
};

struct OpenCommand : public CadCommand {
  OpenCommand() : CadCommand("open") {}
  std::vector<std::string> _filenames;
  virtual void Run(cad_app& app) override;
};

typedef std::shared_ptr<CadCommand> CadCmdPtr;

struct HelixSpec {
  float inner_radius = 2;
  float outer_radius = 3;
  float length = 10;
  float pitch = 3;
  float inner_width = 1.5;
  float outer_width = 0.5;
  bool rod = true;
  bool tube = false;
};

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
  void MakeHelix(const HelixSpec& spec);
 protected:
  UILib* _ui;
  std::shared_ptr<TrigMesh> _floor;
  int _floorMeshId = -1;
  UIConf _conf;
  bool _confChanged = false;
  std::vector<Part> _parts;
  const static size_t MAX_COMMAND_QUEUE_SIZE = 10;
  threadsafe_queue<CadCmdPtr> _commandQueue;
};
