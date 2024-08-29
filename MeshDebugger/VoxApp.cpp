#include "VoxApp.h"
#include "StringUtil.h"
#include "Timer.h"
#include "BBox.h"
#include "MeshUtil.h"
#include "cpu_voxelizer.h"
#include "RaycastConf.h"
#include "ZRaycast.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>

//info for voxelizing
//multi-material interface connector designs
void ConnectorVox::Log(const std::string& str) const {
  if (LogCb) {
    LogCb(str);
  } else {
    std::cout << str << "\n";
  }
}

void ConnectorVox::LoadMeshes() {
  if (filenames.size() == 0) {
    return;
  }
  meshes.resize(filenames.size());
  Log("load meshes");
  for (size_t i = 0; i < filenames.size();i++) {
    const auto& file = filenames[i]; 
    std::string ext = get_suffix(file);
    std::cout << ext << "\n";
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    if (ext.size() != 3) {
      continue;
    }
    if (ext == "obj") {
      meshes[i].LoadObj(file);
    } else if (ext == "stl") {
      meshes[i].LoadStl(file);
    }
  }
  std::filesystem::path filePath(filenames[0]);
  dir = filePath.parent_path().string();
  _fileLoaded = true;
  Log("load meshes done");
}

struct SimpleVoxCb : public VoxCallback {
  SimpleVoxCb(Array3D8u& grid, uint8_t matId) : _grid(grid), _matId(matId) {}
  virtual void operator()(unsigned x, unsigned y, unsigned z,
                          size_t trigIdx) override {
    _grid(x, y, z) = _matId;
  }  
  Array3D8u& _grid;
  uint8_t _matId = 1;
};

void VoxelizeCPU(uint8_t matId, const voxconf& conf, const TrigMesh& mesh,
                 Array3D8u& grid) {
  SimpleVoxCb cb(grid, matId);
  Timer timer;
  timer.start();
  cpu_voxelize_mesh(conf, &mesh, cb);
  timer.end();
  float ms = timer.getMilliseconds();
}

void VoxelizeMesh(uint8_t matId, const BBox& box, float voxRes,
                  const TrigMesh& mesh, Array3D8u& grid) {
  ZRaycast raycast;
  RaycastConf rcConf;
  rcConf.voxelSize_ = voxRes;
  rcConf.box_ = box;
  ABufferf abuf;
  Timer timer;
  timer.start();
  int ret = raycast.Raycast(mesh, abuf, rcConf);
  timer.end();
  float ms = timer.getMilliseconds();
  std::cout << "vox time " << ms << "ms\n";
  timer.start();
  float z0 = box.vmin[2];
  //expand abuf to vox grid
  Vec3u gridSize = grid.GetSize();
  for (unsigned y = 0; y < gridSize[1]; y++) {
    for (unsigned x = 0; x < gridSize[0]; x++) {
      const auto intersections = abuf(x, y);
      for (auto seg : intersections) {
        unsigned zIdx0 = seg.t0 / voxRes + 0.5f;
        unsigned zIdx1 = seg.t1 / voxRes + 0.5f;

        for (unsigned z = zIdx0; z < zIdx1; z++) {
          if (z >= gridSize[2]) {
            continue;
          }
          grid(x, y, z) = matId;
        }
      }
    }
  }
  timer.end();
  ms = timer.getMilliseconds();
  std::cout << "copy time " << ms << "ms\n";
}

void SaveVoxTxt(const Array3D8u & grid, float voxRes, const std::string & filename) {
  std::ofstream out(filename);
  if (!out.good()) {
    std::cout << "cannot open output " << filename << "\n";
    return;
  }
  out << "voxelSize\n" << voxRes << " " << voxRes << " " << voxRes << "\n";
  out << "spacing\n0.2\ndirection\n0 0 1\ngrid\n";
  Vec3u gridsize = grid.GetSize();
  out << gridsize[0] << " " << gridsize[1] << " " << gridsize[2] << "\n";
  for (unsigned z = 0; z < gridsize[2]; z++) {
    for (unsigned y = 0; y < gridsize[1]; y++) {
      for (unsigned x = 0; x < gridsize[0]; x++) {
        out << int(grid(x, y, z) + 1) << " ";
      }
      out << "\n";
    }    
  }
  out << "\n";
}

void ConnectorVox::VoxelizeMeshes() {
  Log("voxelize meshes");
  BBox box;
  for (auto &m:meshes) {
    MergeCloseVertices(m);
    m.ComputePseudoNormals();
    BBox mbox;
    ComputeBBox(m.v, mbox);
    box.Merge(mbox);
  }
  Vec3f boxCenter = 0.5f*(box.vmax + box.vmin);
  for (size_t mi = 0; mi < meshes.size();mi++) {
    auto& m = meshes[mi];
    for (size_t i = 0; i < m.GetNumVerts(); i++) {
      m.v[3 * i] -= boxCenter[0];
      m.v[3 * i+1] -= boxCenter[1];
      m.v[3 * i+2] -= boxCenter[2];
    }
    std::string outdir = conf.workingDir;
    m.SaveObj(outdir + "/" + std::to_string(mi) + ".obj");
  }
  box.vmin = box.vmin - boxCenter;
  box.vmax = box.vmax - boxCenter;
  float voxRes = conf.voxResMM;
  voxconf vconf;
  vconf.unit = Vec3f(voxRes, voxRes, voxRes);

  int band = 0;
  box.vmin = box.vmin - float(band) * vconf.unit;
  box.vmax = box.vmax + float(band) * vconf.unit;
  vconf.origin = box.vmin;

  Vec3f count = (box.vmax - box.vmin) / vconf.unit;
  vconf.gridSize = Vec3u(count[0] + 1, count[1] + 1, count[2] + 1);
  grid.Allocate(count[0], count[1], count[2]);
  grid.Fill(meshes.size());
  for (unsigned i = 0; i < meshes.size(); i++) {
    VoxelizeMesh(uint8_t(i), box, voxRes, meshes[i], grid); 
  }
  _voxelized = true;
  for (unsigned i = 0; i < meshes.size(); i++) {
    uint8_t mat = i;
    SaveVolAsObjMesh(
        dir + "/" + conf.outputFile + std::to_string(i + 1) + ".obj", grid,
                     (float*)(&vconf.unit), i + 1);
  }

  std::string gridFilename = dir + "/" + conf.outputFile;
  SaveVoxTxt(grid, voxRes, gridFilename);
  Log("voxelize meshes done");
}

bool FloatEq(float a, float b, float eps) { return std::abs(a - b) < eps; }

/// @brief 
/// @param widgetId 
/// @param ui 
/// @param confVal 
/// @param inputScale 
/// @return 0 if no change. >0 if value changed. 
int CheckFloatInput(int widgetId, UILib* ui, float & confVal, float inputScale,
  float minVal) {
  std::shared_ptr<InputInt> input =
      std::dynamic_pointer_cast<InputInt>(ui->GetWidget(widgetId));
  float uiVal = confVal;
  if (input && input->_entered) {
    uiVal = float(input->_value * inputScale);
    input->_entered = false;
  }
  const float EPS = 5e-5f;
  uiVal = std::max(minVal, uiVal);
  if (!FloatEq(uiVal, confVal, EPS)) {
    confVal = uiVal;
    return 1;
  }
  return 0;
}

void ConnectorVox::Refresh() {
  //update configs
  int confChanged = 0;
  const float EPS = 5e-5f;
  const float umToMM=0.001f;
  confChanged = CheckFloatInput(_voxResId, _ui, conf.voxResMM, 1e-3f, 1e-2);
  confChanged |= CheckFloatInput(_waxGapId, _ui, conf.waxGapMM, 1e-3f, 0);
  if (_outPrefixId >= 0) {
    std::shared_ptr<InputText> input =
        std::dynamic_pointer_cast<InputText>(_ui->GetWidget(_outPrefixId));
    if (input && input->_entered) {
      input->_entered = false;
    }
    if (conf.outputFile != input->GetString()) {
      conf.outputFile = input->GetString();
      confChanged = true;
    }
  }
  if (confChanged) {
    conf.Save();
  }

  if (_hasFileNames && !_fileLoaded) {
    LoadMeshes();
    _voxelized = false;
  }
  if (_fileLoaded && !_voxelized) {
    VoxelizeMeshes();
  }
}

void OnChangeDir(std::string dir, UIConf& conf) {
  conf.workingDir = dir;
  conf.Save();
}

void LogToUI(const std::string& str, UILib& ui, int statusLabel) {
  ui.SetLabelText(statusLabel, str);
}

void ConnectorVox::Init(UILib* ui) { 
  _ui = ui;
  conf.Load(conf._confFile);
  _ui->AddButton("Open Meshes", [&] { this->OpenMeshes(*_ui, conf); });
  _ui->SetChangeDirCb(
      [&](const std::string& dir) { OnChangeDir(dir, conf); });
  _statusLabel = ui->AddLabel("status");
  LogCb =
      std::bind(LogToUI, std::placeholders::_1, std::ref(*_ui), _statusLabel);
  _resInput =
      std::make_shared<InputInt>("vox res um", int(conf.voxResMM * 1000));
  _voxResId = _ui->AddWidget(_resInput);
  _waxGapInput =
      std::make_shared<InputInt>("wax gap um", int(conf.waxGapMM * 1000));
  _waxGapId = _ui->AddWidget(_waxGapInput);
  _outPrefixId =
      _ui->AddWidget(std::make_shared<InputText>("output file", "grid.txt"));
}

void ConnectorVox::OnOpenMeshes(const std::vector<std::string>& files) {
  filenames = files;
  _fileLoaded = false;
  _hasFileNames = true;
}

void ConnectorVox::OpenMeshes(UILib& ui, const UIConf& conf) {
  ui.SetMultipleFilesOpenCb(std::bind(&ConnectorVox::OnOpenMeshes, this, std::placeholders::_1));
  ui.ShowFileOpen(true, conf.workingDir);
}

void FlipGrid(const std::string& name) {
  Array3D<uint8_t> grid;
  std::ifstream in(name);
  int numHeaders = 8;
  std::vector<std::string> headers(numHeaders);

  for (int i = 0; i < numHeaders; i++) {
    std::getline(in, headers[i]);
  }
  std::istringstream iss(headers.back());
  Vec3u gridSize;
  iss >> gridSize[0] >> gridSize[1] >> gridSize[2];
  grid.Allocate(gridSize[0], gridSize[1], gridSize[2]);

  for (unsigned z = 0; z < gridSize[2]; z++) {
    unsigned dstz = gridSize[2] - z - 1;
    for (unsigned y = 0; y < gridSize[1]; y++) {
      for (unsigned x = 0; x < gridSize[0]; x++) {
        in >> grid(x, y, dstz);
      }
    }
  }

  std::ofstream out("grid_flipz.txt");
  for (int i = 0; i < numHeaders; i++) {
    out << headers[i] << "\n";
  }
  for (unsigned z = 0; z < gridSize[2]; z++) {
    for (unsigned y = 0; y < gridSize[1]; y++) {
      for (unsigned x = 0; x < gridSize[0]; x++) {
        out << grid(x, y, z) << " ";
      }
      out << "\n";
    }
  }
}