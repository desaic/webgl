#include "PackApp.hpp"
#include "ImageUtils.h"
#include "StringUtil.h"
#include "imgui.h"
#include "BBox.h"
#include "Matrix3f.h"
#include "Quat4f.h"
#include "Transformations.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

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

  _ui->AddButton("Load Sim", [&]() {
    auto meshOpenCb =
        std::bind(&PackApp::QueueLoadSim, this, std::placeholders::_1);
    _ui->SetFileOpenCb(meshOpenCb);
    bool openMulti = false;
    _ui->ShowFileOpen(openMulti, _conf.workingDir);
  });

  _ui->AddButton("one step", [&]() { rigidStates.stepState = 1; });
  _ui->AddButton(
      "play sim", [&]() { rigidStates.stepState = 2; }, true);
  _ui->AddButton(
      "reset sim",
      [&]() {
        rigidStates.currStep = 0;
        rigidStates.stepState = 0;
      },
      true);
  _simStepLabel = _ui->AddLabel("sim step 0");
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
  lights->SetLightPos(0, 0, 300, 300);
  lights->SetLightPos(1, 0, 300, -300);
  lights->SetLightColor(0, 1,1,1);
  lights->SetLightColor(1, 0.8, 0.8, 0.8);
  //lights->SetLightPos(2, 0, 100, 100);
  //lights->SetLightColor(2, 0.8, 0.7, 0.6);
}

void PackApp::OnChangeDir(std::string dir) {
  _conf.workingDir = dir;
  _confChanged = true;
}

static int LoadMeshFile(const std::string& path, TrigMesh& mesh) {
  std::string suffix = get_suffix(path);
  for (char& c : suffix) {
    c = std::tolower(c);
  }
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

void PackApp::QueueLoadSim(const std::string& stateFile) {
  QueueCommand(std::make_shared<LoadSimCmd>(this, stateFile));
}

void PackApp::AddSimMeshesToUI() {
  rigidStates.uiInstIds.resize(rigidStates.meshes.size());
  for (size_t i = 0; i < rigidStates.meshes.size(); i++) {
    int instId = _ui->AddMeshAndInstance(rigidStates.meshes[i]);
    rigidStates.uiInstIds[i] = instId;
  }
}

std::vector<std::shared_ptr<TrigMesh > > LoadSimMesh(const std::filesystem::path& p) {
  std::ifstream in(p);
  if (!in.good()) {
    return {};
  }
  size_t vOffset = 1;
  std::string line;
  std::shared_ptr<TrigMesh> mesh;
  std::vector<std::shared_ptr<TrigMesh> > meshes; 
  while (std::getline(in, line)) {

    if (line.size() < 3 || line[0] == '#') {
      continue;
    }
    std::istringstream iss(line);
    std::string tok;
    iss >> tok;
    if (tok[0] == 'o') {
      if (mesh) {
        meshes.push_back(mesh);
        vOffset += mesh->GetNumVerts();
        std::string debugFile =
            "./debug_mesh_" + std::to_string(meshes.size()) + ".obj";
        //mesh->SaveObj(debugFile);
      }
      mesh = std::make_shared<TrigMesh>();
    } else if (tok[0] == 'v') {
      if (mesh) {
        Vec3f vert;
        iss >> vert[0] >> vert[1] >> vert[2];
        mesh->AddVert(vert[0], vert[1], vert[2]);
      }
    } else if (tok[0] == 'f') {
      if (mesh) {
        Vec3u face;
        iss >> face[0] >> face[1] >> face[2];
        mesh->t.push_back(face[0] - vOffset);
        mesh->t.push_back(face[1] - vOffset);
        mesh->t.push_back(face[2] - vOffset);
      }
    }
  }
  meshes.push_back(mesh);
  vOffset += mesh->GetNumVerts();
  //std::string debugFile =
  //    "./debug_mesh_" + std::to_string(meshes.size()) + ".obj";
  //mesh->SaveObj(debugFile);
  return meshes;
}

void PackApp::LoadSim(const std::string& stateFile) { 
  std::ifstream in(stateFile);
  if (!in.good()) {
    std::cout << "can't open " << stateFile << "\n";
    return;
  }
  std::string line;
  std::getline(in, line);
  std::string token;
  std::istringstream iss(line);
  std::string meshFile;
  iss >> token >> meshFile;

  std::filesystem::path p(stateFile);
  auto dir = p.parent_path();
  std::filesystem::path meshPath = dir / meshFile;
  rigidStates.meshes = LoadSimMesh(meshPath);
  if (rigidStates.meshes.size() == 0) {
    return;
  }
  int numSteps = 0;
  in >> token >> numSteps;
  rigidStates.states.resize(numSteps);
  for (int i = 0; i < numSteps; i++) {
    std::getline(in, line);
    int stepId;
    int numBodies = 0;
    in >> token >> stepId >> token >> numBodies;
    RigidState s;
    s.resize(numBodies);
    for (int j = 0; j < numBodies; j++) {
      in >> s[j].pos[0] >> s[j].pos[1] >> s[j].pos[2];
      in >> s[j].rot[0] >> s[j].rot[1] >> s[j].rot[2] >> s[j].rot[3];
    }
    rigidStates.states[i] = s;
  }
  AddSimMeshesToUI();
}

int PackApp::GetUISliderVal(int widgetId) const {
  if (widgetId >= 0) {
    std::shared_ptr<const Slideri> slider =
        std::dynamic_pointer_cast<const Slideri>(_ui->GetWidget(widgetId));
    return slider->GetVal();
  }
  return -1;
}

/// @brief 
/// @param pos 
/// @param rot quaternion in wxyz order
/// @return 
Matrix4f RigidMatrix(const Vec3f & pos, const Vec4f&rot) {  
  Quat4f q(rot[0], rot[1],rot[2],rot[3]);
  Matrix4f mat = Matrix4f::rotation(q);
  mat(0, 3) = pos[0];
  mat(1, 3) = pos[1];
  mat(2, 3) = pos[2];
  return mat;
}

void PackApp::UpdateSimRender() {
  if (rigidStates.states.size() == 0) {
    return;
  }
  GLRender* gl = _ui->GetGLRender();
  int step = rigidStates.currStep;
  if (step < 0) {
    step = 0;
  }
  if (step >= rigidStates.states.size()) {
    step = rigidStates.states.size() - 1;
    //stop sim after reaching the end.
    rigidStates.stepState = 0;
  }  
  
  const auto& s = rigidStates.states[step];
  //loop over bodies
  for (size_t b = 0; b < rigidStates.meshes.size(); b++) {
    int instId = rigidStates.uiInstIds[b];
    if (instId < 0) {
      continue;
    }
    auto& inst = gl->GetInstance(instId);
    inst.matrix = RigidMatrix(s[b].pos, s[b].rot);
    inst.matrixUpdated = true;
  }    
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

  if (rigidStates.stepState == 1) {
    rigidStates.currStep++;
    rigidStates.stepState = 0;
    UpdateSimRender();
  } else if (rigidStates.stepState == 2) {
    rigidStates.currStep++;
    UpdateSimRender();
  }
  if (_simStepLabel >= 0) {
    _ui->SetLabelText(_simStepLabel,
                      "sim step " + std::to_string(rigidStates.currStep));
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
void LoadSimCmd::Run() { app->LoadSim(_filename); }