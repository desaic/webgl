#pragma once
#include "Command.hpp"
#include "TrigMesh.h"
#include "UIConf.h"
#include "UILib.h"
#include "Vec4.h"
#include "threadsafe_queue.h"
#include "MakeHelix.h"

// axis aligned cuboid orientation.
// e.g. yzx means put y axis of the cube in x axis of the world.
enum class CUBE_ORIENT { XYZ, XZY, YZX, YXZ, ZXY, ZYX, NUM_ORIENT };

struct Placement {
  Placement() : pos(0), o(CUBE_ORIENT::XYZ) {}
  Placement(Vec3u p, CUBE_ORIENT ori) : pos(p), o(ori) {}
  Vec3u pos;
  CUBE_ORIENT o = CUBE_ORIENT(0);
};

class PackApp;

struct LoadContainerCmd : public Command {
  LoadContainerCmd() : Command("container") {}
  LoadContainerCmd(PackApp* a, const std::string& f)
      : Command("container"), app(a), _filename(f) {}
  virtual void Run() override;
  PackApp* app = nullptr;
  std::string _filename;
};

struct LoadBoxesCmd : public Command {
  LoadBoxesCmd() : Command("boxes") {}
  LoadBoxesCmd(PackApp* a, const std::string& f)
      : Command("boxes"), app(a), _filename(f) {}
  virtual void Run() override;
  PackApp* app = nullptr;
  std::string _filename;
};

struct LoadSimCmd : public Command {
  LoadSimCmd() : Command("load_sim") {}
  LoadSimCmd(PackApp* a, const std::string& f)
      : Command("load_sim"), app(a), _filename(f) {}
  virtual void Run() override;
  PackApp* app = nullptr;
  std::string _filename;
};


struct MotionState {
  Vec3f pos;
  Vec4f rot;
};

typedef std::vector<MotionState> RigidState;

struct RigidSimState {
  std::vector<std::shared_ptr<TrigMesh>> meshes; 
  std::vector<int> uiInstIds;
  std::vector<RigidState> states;

  // stop 0, single step 1, run 2.
  int stepState = 0;
  int currStep = 0;
};

class PackApp {
 public:
  void Init(UILib* ui);
  void Refresh();

  void ExeCommands();

  void OnChangeDir(std::string dir);

  void QueueCommand(CmdPtr cmd);

  void QueueLoadContainer(const std::string& file);
  void LoadContainer(const std::string& file);

  void QueueLoadBoxes(const std::string & seqFile);
  void LoadBoxes(const std::string& seqFile);

  void QueueLoadSim(const std::string& stateFile);
  void LoadSim(const std::string& seqFile);

  int GetUISliderVal(int widgetId) const;
 protected:
  UILib* _ui;
  std::shared_ptr<TrigMesh> _floor;
  std::shared_ptr<TrigMesh> _container;
  std::shared_ptr<TrigMesh> _boxMesh;
  int _floorInst = -1;
  int _containerInst = -1;

  int _showContainerCheckBox = -1;

  int _startBoxSlider = -1;
  int _endBoxSlider = -1;
  int _simStepLabel = -1;
  UIConf _conf;
  bool _confChanged = false;
  const static size_t MAX_COMMAND_QUEUE_SIZE = 10;
  threadsafe_queue<CmdPtr> _commandQueue;
  std::shared_ptr<InputText> _outDirWidget;
  std::vector<Placement> _places;
  std::vector<int> _instIds;
  Vec3f _boxSize = {};

  RigidSimState rigidStates;
  void Init3DScene(UILib* ui);
  void AddSimMeshesToUI();
  void UpdateSimRender();
};