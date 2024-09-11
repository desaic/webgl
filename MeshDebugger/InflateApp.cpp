#include "InflateApp.h"
#include "StringUtil.h"
#include "Stopwatch.h"
#include "BBox.h"
#include "AdapUDF.h"
#include "MeshUtil.h"
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
  confChanged |= CheckFloatInput(_thicknessId, _ui, conf.thicknessMM, 1, 0);
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

void ComputeCoarseDist(AdapDF*df) {
  df->ClearTemporaryArrays();
  Utils::Stopwatch timer;
  timer.Start();
  df->BuildTrigList(df->mesh_);
  float ms = timer.ElapsedMS();
  timer.Start();
  df->Compress();
  ms = timer.ElapsedMS();
  timer.Start();
  df->mesh_->ComputePseudoNormals();
  ms = timer.ElapsedMS();
  timer.Start();
  df->ComputeCoarseDist();
  ms = timer.ElapsedMS();
  Array3D8u frozen;
  timer.Start();
  df->FastSweepCoarse(frozen);
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
  mesh.ComputeTrigNormals();
  BBox box;
  ComputeBBox(mesh.v, box);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";
  float h = conf.voxResMM;
  const unsigned MAX_GRID_SIZE = 1000;
  std::shared_ptr<AdapDF> udf = std::make_shared<AdapUDF>();
  for (size_t d = 0; d < 3; d++) {
    float len = box.vmax[d] - box.vmin[d];
    if (len / h > MAX_GRID_SIZE) {
      h = len / MAX_GRID_SIZE;
    }
  }
  udf->distUnit = 1e-2;
  udf->voxSize = h;
  udf->band = conf.thicknessMM / h + 1;
  udf->mesh_ = &mesh;
  ComputeCoarseDist(udf.get());
  Array3D8u outside;
  outside = FloodOutside(udf->dist, h / udf->distUnit);
  Vec3f voxRes(h);
  //SaveVolAsObjMesh("F:/dump/inflate_flood.obj", frozen, (float*)(&voxRes), 1);
  for (size_t i = 0; i < outside.GetData().size(); i++) {
    if (!outside.GetData()[i]) {
      udf->dist.GetData()[i] = -1;
    }
  }
  //SaveSlice(udf->dist, udf->dist.GetSize()[2] / 2, udf->distUnit);
  TrigMesh surf;
  SDFImpAdap dfImp(udf);
  dfImp.MarchingCubes(conf.thicknessMM, &surf);
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


static size_t LinearIdx(unsigned x, unsigned y, unsigned z, const Vec3u& size) {
  return x + y * size_t(size[0]) + z * size_t(size[0] * size[1]);
}

static Vec3u GridIdx(size_t l, const Vec3u& size) {
  unsigned x = l % size[0];
  unsigned y = (l % (size[0] * size[1])) / size[0];
  unsigned z = l / (size[0] * size[1]);
  return Vec3u(x, y, z);
}
static bool InBound(int x, int y, int z, const Vec3u& size) {
  return x >= 0 && y >= 0 && z >= 0 && x < int(size[0]) && y < int(size[1]) &&
         z < int(size[2]);
}

static void FloodOutsideSeed(Vec3u seed, const Array3D<short>& dist, float distThresh,
                      Array3D8u& label) {
  Vec3u size = dist.GetSize();
  size_t linearSeed = LinearIdx(seed[0], seed[1], seed[2], size);
  std::deque<size_t> q(1, linearSeed);
  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                               {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  while (!q.empty()) {
    size_t linearIdx = q.front();
    q.pop_front();
    Vec3u idx = GridIdx(linearIdx, size);
    label(idx[0], idx[1], idx[2]) = 1;
    for (unsigned ni = 0; ni < NUM_NBR; ni++) {
      int nx = int(idx[0] + nbrOffset[ni][0]);
      int ny = int(idx[1] + nbrOffset[ni][1]);
      int nz = int(idx[2] + nbrOffset[ni][2]);
      if (!InBound(nx, ny, nz, size)) {
        continue;
      }
      unsigned ux = unsigned(nx), uy = unsigned(ny), uz = unsigned(nz);
      uint8_t nbrLabel = label(ux, uy, uz);
      size_t nbrLinear = LinearIdx(ux, uy, uz, size);
      if (nbrLabel == 0) {
        uint16_t nbrDist = dist(ux, uy, uz);
        if (nbrDist >= distThresh) {
          label(ux, uy, uz) = 1;
          q.push_back(nbrLinear);
        }
      }
    }
  }
}

/// <param name="dist">distance grid. at least 1 voxel of padding is
/// assumed.</param> <param name="distThresh">stop flooding if distance is less
/// than this</param> <returns>voxel labels. 1 for outside.</returns>
static Array3D8u FloodOutside(const Array3D<short>& dist, float distThresh) {
  Array3D8u label;
  Vec3u size = dist.GetSize();
  label.Allocate(size[0], size[1], size[2]);
  label.Fill(0);
  // process easy voxels first.

  // all 6 faces are labeled to be outside regardless of actual distance
  // assuming the voxel grid has padding around the mesh.
  // all threads hanging from the faces are also labeld as outside quickly.
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      if (y == 0 || y == size[1] - 1 || z == 0 || z == size[2] - 1) {
        for (unsigned x = 0; x < size[0]; x++) {
          label(x, y, z) = 1;
        }
        continue;
      }
      label(0, y, z) = 1;
      label(size[0] - 1, y, z) = 1;
      for (unsigned x = 1; x < size[0] - 1; x++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned x = size[0] - 1; x > 0; x--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  for (unsigned x = 0; x < size[0]; x++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned z = 1; z < size[2] - 1; z++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned z = size[2] - 1; z > 0; z--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned x = 0; x < size[0]; x++) {
      for (unsigned y = 1; y < size[1] - 1; y++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned y = size[1] - 1; y > 0; y--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{-1, 0, 0}, {0, -1, 0}, {0, 0, -1},
                               {1, 0, 0},  {0, 1, 0},  {0, 0, 1}};
  for (unsigned z = 1; z < size[2] - 1; z++) {
    for (unsigned y = 1; y < size[1] - 1; y++) {
      for (unsigned x = 1; x < size[0] - 1; x++) {
        for (unsigned ni = 0; ni < NUM_NBR; ni++) {
          unsigned ux = unsigned(x + nbrOffset[ni][0]);
          unsigned uy = unsigned(y + nbrOffset[ni][1]);
          unsigned uz = unsigned(z + nbrOffset[ni][2]);

          if (!label(x, y, z)) {
            continue;
          }
          uint8_t nbrLabel = label(ux, uy, uz);
          if (nbrLabel == 0) {
            uint16_t nbrDist = dist(ux, uy, uz);
            if (nbrDist >= distThresh) {
              FloodOutsideSeed(Vec3u(x, y, z), dist, distThresh, label);
            }
          }
        }
      }
    }
  }
  return label;
}
