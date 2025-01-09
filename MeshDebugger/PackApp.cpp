#include "PackApp.hpp"
#include "ImageUtils.h"
#include "StringUtil.h"
#include "imgui.h"
#include "BBox.h"
#include "Matrix3f.h"
#include "Transformations.h"
#include <algorithm>
#include <fstream>
#include <sstream>

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
  _showContainerCheckBox = _ui->AddCheckBox("show container", true);
  _ui->AddButton("Boxes", [&]() {
    auto meshOpenCb =
        std::bind(&PackApp::QueueLoadBoxes, this, std::placeholders::_1);
    _ui->SetFileOpenCb(meshOpenCb);
    bool openMulti = false;
    _ui->ShowFileOpen(openMulti, _conf.workingDir);
  });

  _startBoxSlider = _ui->AddSlideri("start i", 0, 0, 1000);
  _endBoxSlider = _ui->AddSlideri("end i", 1000, 0, 1000);

  _ui->SetChangeDirCb(
      std::bind(&PackApp::OnChangeDir, this, std::placeholders::_1));

  _outDirWidget = std::make_shared<InputText>("out dir", _conf.outDir);
  _ui->AddWidget(_outDirWidget);
  
}

void PackApp::Init3DScene(UILib*ui) {
  
  _floor = std::make_shared<TrigMesh>();
  *_floor =
      MakePlane(Vec3f(-50, -0.1, -50), Vec3f(50, -0.1, 50), Vec3f(0, 1, 0));
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

void PackApp::QueueLoadBoxes(const std::string& seqFile) {
  QueueCommand(std::make_shared<LoadBoxesCmd>(this, seqFile));
}

Matrix3f CubeRotMat(CUBE_ORIENT o) {
  Matrix3f mat = Matrix3f::identity();
  switch (o) {
    case CUBE_ORIENT::XYZ:      
      break;
    case CUBE_ORIENT::XZY:
      mat.setRow(1, Vec3f(0, 0, -1));
      mat.setRow(2, Vec3f(0, 1,  0));
      break;
    case CUBE_ORIENT::YZX:
      mat.setRow(0, Vec3f(0, 1, 0));
      mat.setRow(1, Vec3f(0, 0, 1));
      mat.setRow(2, Vec3f(1, 0, 0));
      break;
    case CUBE_ORIENT::YXZ:
      mat.setRow(0, Vec3f(0, -1, 0));
      mat.setRow(1, Vec3f(1, 0, 0));
      break;
    case CUBE_ORIENT::ZXY:
      mat.setRow(0, Vec3f(0, 0, 1));
      mat.setRow(1, Vec3f(1, 0, 0));
      mat.setRow(2, Vec3f(0, 1, 0));
      break;
    case CUBE_ORIENT::ZYX:
      mat.setRow(0, Vec3f(0, 0, -1));
      mat.setRow(2, Vec3f(1, 0, 0));
      break;
  }
  return mat;
}

void PackApp::LoadBoxes(const std::string& seqFile) {
  std::ifstream in(seqFile);
  if (!in.good()) {
    return;
  }
  
  std::string line;
  std::string token;

  in >> token;
  in >> _boxSize[0] >> _boxSize[1] >> _boxSize[2];
  _places.clear();
  _instIds.clear();
  while (std::getline(in, line)) {
    if (line.size() < 7) {
      continue;
    }
    std::istringstream iss(line);
    Placement p;
    iss >> p.pos[0] >> p.pos[1] >> p.pos[2];
    int o;
    iss >> o;
    p.o = CUBE_ORIENT(o);
    _places.push_back(p);
  }
  in.close();
  _boxMesh = std::make_shared<TrigMesh>();
  *_boxMesh = MakeCube(-0.5f * _boxSize, 0.5f * _boxSize);
  GLRender* gl = _ui->GetGLRender();
  int bufId = gl->CreateMeshBufs(_boxMesh);
  for (size_t i = 0; i < _places.size(); i++) {
    Matrix3f rot = CubeRotMat(_places[i].o);
    std::vector<float> rotated(_boxMesh->v.size());
    transformVerts(_boxMesh->v, rotated, rot);
    BBox box;
    ComputeBBox(rotated, box);
    
    Vec3f pos(_places[i].pos[0], _places[i].pos[1], _places[i].pos[2]);
    Vec3f disp = -box.vmin + pos;
    Matrix4f instTrans;
    instTrans.setSubmatrix3x3(0, 0, rot);
    instTrans(0, 3) = disp[0];
    instTrans(1, 3) = disp[1];
    instTrans(2, 3) = disp[2];
    instTrans(3, 3) = 1;
    MeshInstance instance;
    instance.bufId = bufId;
    instance.matrix = instTrans;
    int id = gl->AddInstance(instance);
    _instIds.push_back(id);
  }
  
  if (_startBoxSlider >= 0 && _endBoxSlider >= 0) {
    std::shared_ptr<Slideri> startWidget =
        std::dynamic_pointer_cast<Slideri>(_ui->GetWidget(_startBoxSlider));
    startWidget->SetUb(_places.size());
    std::shared_ptr<Slideri>  endWidget =
        std::dynamic_pointer_cast<Slideri>(_ui->GetWidget(_endBoxSlider));
    endWidget->SetUb(_places.size());
    endWidget->SetVal(_places.size());
  }

  for (size_t i = 0; i < _boxMesh->v.size(); i++) {
    _boxMesh->v[i] *= 0.98;
  }
  gl->SetNeedsUpdate(bufId);
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

int PackApp::GetUISliderVal(int widgetId) const {
  if (widgetId >= 0) {
    std::shared_ptr<const Slideri> slider =
        std::dynamic_pointer_cast<const Slideri>(_ui->GetWidget(widgetId));
    return slider->GetVal();
  }
  return -1;
}

void PackApp::Refresh() {
  ExeCommands();
  if (_containerInst >= 0) {
    MeshInstance & inst = _ui->GetGLRender()->GetInstance(_containerInst);
    bool show = _ui->GetCheckBoxVal(_showContainerCheckBox);
    inst.hide = !show;
  }
  if (_startBoxSlider >= 0 && _endBoxSlider >= 0) {
    GLRender* gl = _ui->GetGLRender();
    int startIdx = GetUISliderVal(_startBoxSlider);
    int endIdx = GetUISliderVal(_endBoxSlider);
    if (endIdx < 0) {
      endIdx = _instIds.size();
    }
    if (endIdx < startIdx) {
      endIdx = startIdx;
    }
    for (size_t i = 0; i < _instIds.size(); i++) {
      bool hide = true;
      if (i >= startIdx && i <= endIdx) {
        hide = false;
      }
      auto& inst = gl->GetInstance(_instIds[i]);
      inst.hide = hide;
    }
  }

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
void LoadBoxesCmd::Run() { app->LoadBoxes(_filename); }