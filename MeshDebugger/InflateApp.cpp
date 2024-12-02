#include "InflateApp.h"
#include "StringUtil.h"
#include "Stopwatch.h"
#include "BBox.h"
#include "AdapUDF.h"
#include "MeshUtil.h"
#include "Inflate.h"
#include "cpu_voxelizer.h"
#include "RaycastConf.h"
#include "ZRaycast.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>

static Array3D8u FloodOutside(const Array3D<short>& dist, float distThresh) ;

//info for voxelizing
//multi-material interface connector designs
void InflateApp::Log(const std::string& str) const {
  if (LogCb) {
    LogCb(str);
  } else {
    std::cout << str << "\n";
  }
}

void InflateApp::LoadMeshes() {
  if (filenames.size() == 0) {
    return;
  }
  meshes.clear();
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
  filePrefix = filePath.filename().replace_extension().string();  
  _fileLoaded = true;
  Log("load meshes done");
}

static bool FloatEq(float a, float b, float eps) { return std::abs(a - b) < eps; }

/// @brief 
/// @param widgetId 
/// @param ui 
/// @param confVal 
/// @param inputScale 
/// @return 0 if no change. >0 if value changed. 
static int CheckFloatInput(int widgetId, UILib* ui, float & confVal, float inputScale,
  float minVal) {
  std::shared_ptr<InputFloat> input =
      std::dynamic_pointer_cast<InputFloat>(ui->GetWidget(widgetId));
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

void InflateApp::Refresh() {
  //update configs
  int confChanged = 0;
  const float EPS = 5e-5f;
  confChanged = CheckFloatInput(_voxResId, _ui, conf.voxResMM, 1, 1e-2);
  confChanged |= CheckFloatInput(_thicknessId, _ui, conf.thicknessMM, 1, -1);
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
    _inflated = false;
  }
  if (_fileLoaded && !_inflated) {
    InflateMeshes();
    _inflated = true;
  }
}

void static OnChangeDir(std::string dir, UIConf& conf) {
  conf.workingDir = dir;
  conf.Save();
}

void static LogToUI(const std::string& str, UILib& ui, int statusLabel) {
  ui.SetLabelText(statusLabel, str);
}

void InflateApp::Init(UILib* ui) { 
  _ui = ui;
  conf.Load(conf._confFile);
  _ui->AddButton("Open Meshes", [&] { this->OpenMeshes(*_ui, conf); });
  _ui->SetChangeDirCb(
      [&](const std::string& dir) { OnChangeDir(dir, conf); });
  _statusLabel = ui->AddLabel("status");
  LogCb =
      std::bind(LogToUI, std::placeholders::_1, std::ref(*_ui), _statusLabel);
  _resInput = std::make_shared<InputFloat>("vox res mm", conf.voxResMM);
  _voxResId = _ui->AddWidget(_resInput);
  _thicknessInput =
      std::make_shared<InputFloat>("thickness mm", conf.thicknessMM);
  _thicknessId = _ui->AddWidget(_thicknessInput);
  _outPrefixId =
      _ui->AddWidget(std::make_shared<InputText>("output file", "inflate.obj"));
}

#include <iostream>
#include "ImageIO.h"
// debug function
template <typename T>
void SaveSlice(Array3D<T>& dist, unsigned z, float unit) {
  Vec3u size = dist.GetSize();
  if (size[2] == 0) {
    return;
  }
  z = std::min(size[2] - 1, z);
  Array2D8u image(size[0], size[1]);
  for (unsigned y = 0; y < size[1]; y++) {
    for (unsigned x = 0; x < size[0]; x++) {
      float d = dist(x, y, z) * unit;
      if (dist(x, y, z) > 32766) {
        image(x, y) = 255;
        continue;
      }
      if (dist(x, y, z) < -32767) {
        image(x, y) = 0;
        continue;
      }
      std::cout << d << "\n";
      if (d >= 0) {
        image(x, y) = uint8_t(d + 127);
      } else {
        image(x, y) = uint8_t(d);
        // std::cout << x<<" "<<y<<" "<<z<<" "<<d << "\n";
      }
    }
  }
  std::string outName = "F:/dump/dist" + std::to_string(z) + ".png";
  SavePngGrey(outName, image);
}

void InflateApp::InflateMeshes() {
  Log("inflate meshes");
  if (meshes.size() == 0) {
    return;
  }
  TrigMesh mesh = meshes[0];
  for (size_t i = 1; i < meshes.size(); i++) {
    size_t t0 = mesh.t.size();
    size_t trigOffset = mesh.GetNumTrigs();
    mesh.v.insert(mesh.v.end(), meshes[i].v.begin(), meshes[i].v.end());
    mesh.t.insert(mesh.t.end(), meshes[i].t.begin(), meshes[i].t.end());
    for (size_t t = t0; t < mesh.t.size(); t++) {
      mesh.t[t] += trigOffset;
    }
  }
  unsigned MAX_GRID_SIZE = 1000;

  std::string suffix = ".obj";
  std::string inputSuffix = "";
  std::string input = conf.outputFile;
  if (input.size() > 4) {
    inputSuffix = input.substr(input.size() - 4);
    std::transform(inputSuffix.begin(), inputSuffix.end(), inputSuffix.begin(),
                   [](unsigned char c) { return std::tolower(c); });
  }
  if (inputSuffix == ".obj") {
    suffix = "";
  }
  std::string outpath = dir + "/" + filePrefix + input + suffix;
  TrigMesh surf;
  InflateConf inflateConf;
  inflateConf.voxResMM = conf.voxResMM;
  inflateConf.thicknessMM = conf.thicknessMM;
  surf = InflateMesh(inflateConf, mesh);
  surf.SaveObj(outpath);
}

void InflateApp::OnOpenMeshes(const std::vector<std::string>& files) {
  filenames = files;
  _fileLoaded = false;
  _hasFileNames = true;
}

void InflateApp::OpenMeshes(UILib& ui, const UIConf& conf) {
  ui.SetMultipleFilesOpenCb(
      std::bind(&InflateApp::OnOpenMeshes, this, std::placeholders::_1));
  ui.ShowFileOpen(true, conf.workingDir);
}
