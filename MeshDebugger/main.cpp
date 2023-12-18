
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include "Array2D.h"
#include "Array3D.h"
#include "BBox.h"
#include "cpu_voxelizer.h"
#include "MeshUtil.h"
#include "ImageIO.h"
#include "TrigMesh.h"
#include "UIConf.h"
#include "UILib.h"
#include "lodepng.h"
#include "Timer.h"
#include "ZRaycast.h"
#include "RaycastConf.h"
#include "TestNanoVdb.h"

std::vector<Vec3b> generateRainbowPalette(int numColors);

struct UVState {
  UVState() {}
  UVState(TrigMesh& m) : mesh(&m) { Init(); }

  void Init();

  TrigMesh* mesh = nullptr;
  std::vector<uint32_t> trigLabels;
  /// temporary variable. overwritten by each chart.
  std::vector<uint8_t> vertLabels;
  /// <summary>
  /// Maps from vertex index to list of triangle indices.
  /// </summary>
  std::vector<std::vector<uint32_t> > vertToTrig;
  /// <summary>
  /// Per-triangle uv coordinates.
  /// </summary>
  std::vector<Vec2f> uv;

  uint32_t numCharts = 0;
  /// list of triangles for each chart
  std::vector<std::vector<uint32_t> > charts;

  float distortionBound = 2.0f;

  // debug states
  std::vector<unsigned> trigLevel;

  // temporary vertex uv. can be overwritten by new charts.
  std::vector<Vec2f> tmpUV;
  std::vector<unsigned> rejectedVerts;
};

void FillRainbowColor(Array2D8u& image) {
  Vec2u size = image.GetSize();
  size_t numPix = (size[0] / 4) * size[1];
  std::vector<Vec3b> colors = generateRainbowPalette(int(numPix));
  auto& data = image.GetData();
  for (size_t i = 0; i < data.size() / 4; i++) {
    size_t colorIdx = i % colors.size();
    data[4 * i] = colors[colorIdx][0];
    data[4 * i + 1] = colors[colorIdx][1];
    data[4 * i + 2] = colors[colorIdx][2];
    data[4 * i + 3] = 255;
  }
}

struct DebuggerState {
  DebuggerState() {
    trigLabelImage.Allocate(4096, 1024);
    FillRainbowColor(trigLabelImage);
    trigLabelImage(0, 0) = 255;
    trigLabelImage(1, 0) = 0;
    trigLabelImage(2, 0) = 0;
  }
  unsigned displayTrigLevel = 50;
  int sliderId = 0;
  std::shared_ptr<Slideri> levelSlider;
  std::shared_ptr<CheckBox> trigLabelCheckBox;
  std::vector<Vec2f> uv;
  UVState uvState;
  void SetMesh(TrigMesh* m) {
    uvState.mesh = m;
    uvState.Init();
    uv = m->uv;
  }
  Array2D8u trigLabelImage;
  Array2D8u texImage;
  bool showTrigLabels = false;

  UILib* ui = nullptr;
  int meshId = 0;
  void Refresh();
  void LevelSliderChanged();
};

void ShowTrigsUpToLevel(unsigned maxlevel, DebuggerState& debState);
void ShowGLInfo(UILib& ui, int label_id) {
  std::string info = ui.GetGLInfo();
  ui.SetLabelText(label_id, info);
}

void TestImageDisplay(UILib& ui) {
  Array2D8u image(300, 100);
  int imageId = ui.AddImage();
  image.Fill(128);
  Vec2u size = image.GetSize();
  Array2D8u imageA(size[0] / 3 * 4, size[1]);
  for (size_t row = 0; row < size[1]; row++) {
    for (size_t col = 0; col < size[0] / 3; col++) {
      imageA(4 * col, row) = image(3 * col, row);
      imageA(4 * col + 1, row) = image(3 * col + 1, row);
      imageA(4 * col + 2, row) = image(3 * col + 2, row);
      imageA(4 * col + 3, row) = 255;
    }
  }
  ui.SetImageData(imageId, image, 3);
}

void UVState::Init() {
  size_t numTrigs = mesh->GetNumTrigs();
  size_t numVerts = mesh->GetNumVerts();
  trigLabels.clear();
  trigLabels.resize(numTrigs, 0);
  vertToTrig.clear();
  vertToTrig.resize(numVerts);
  for (size_t i = 0; i < numTrigs; i++) {
    for (size_t j = 0; j < 3; j++) {
      size_t vIdx = mesh->t[3 * i + j];
      vertToTrig[vIdx].push_back(i);
    }
  }
  if (mesh->nt.size() != mesh->t.size()) {
    mesh->ComputeTrigNormals();
  }
  numCharts = 0;
  charts.clear();
  vertLabels.clear();
  vertLabels.resize(numVerts, 0);
  tmpUV.resize(numVerts);
  trigLevel.resize(numTrigs, 0);
  uv = mesh->uv;
}

void LoadUVSeg(const std::string& segFile, UVState& state) {
  std::ifstream in(segFile);
  std::string line;
  std::vector<unsigned> chartId;
  std::vector<unsigned> level;
  unsigned numTrigs = 0;
  unsigned trigId = 0;
  unsigned maxChartId = 0;
  while (std::getline(in, line)) {
    if (line.size() == 0) {
      continue;
    }
    if (line[0] == '#') {
      continue;
    }
    std::istringstream ss(line);
    if (line.find("num_trigs") != std::string::npos) {
      std::string tok;
      ss >> tok >> numTrigs;
      if (numTrigs > 0 || numTrigs < state.mesh->GetNumTrigs()) {
        chartId.resize(numTrigs);
        level.resize(numTrigs);
      } else {
        std::cout << "invalid num trigs " << numTrigs << "\n";
        return;
      }
    } else {
      ss >> chartId[trigId] >> level[trigId];
      maxChartId = std::max(chartId[trigId], maxChartId);
      trigId++;
    }
  }
  state.trigLevel = level;
  state.charts.resize(maxChartId + 1);
  for (size_t t = 0; t < chartId.size(); t++) {
    state.charts[chartId[t]].push_back(t);
  }
  state.trigLabels = chartId;
}

std::vector<Vec3b> generateRainbowPalette(int numColors) {
  std::vector<Vec3b> colors;

  float frequency =
      M_PI /
      numColors;  // Controls the frequency of the rainbow (adjust as needed)
  for (int i = 0; i < numColors; ++i) {
    int red = static_cast<uint8_t>(std::sin(frequency * i) * 127 + 128);
    int green = static_cast<uint8_t>(
        std::sin(frequency * i + 2 * M_PI / 3) * 127 + 128);
    int blue = static_cast<uint8_t>(
        std::sin(frequency * i + 4 * M_PI / 3) * 127 + 128);
    colors.push_back(Vec3b(red, green, blue));
  }

  return colors;
}

void MakePerPixelUV(const std::vector<uint32_t> & trigLabels, TrigMesh& mesh,
                    unsigned width) {
  unsigned numTrig = mesh.GetNumTrigs();
  float pixSize = 1.0f / width;
  for (unsigned t = 0; t < numTrig; t++) {
    unsigned l = (trigLabels[t] * 100000)%(width*width);
    unsigned row = l / width;
    unsigned col = l % width;
    Vec2f uv((col + 0.5f) * pixSize, (row + 0.5f) * pixSize);
    mesh.uv[3 * t] = uv;
    mesh.uv[3 * t + 1] = uv;
    mesh.uv[3 * t + 2] = uv;
  }
}

void DebuggerState::LevelSliderChanged() {
  displayTrigLevel = levelSlider->GetVal();
  ShowTrigsUpToLevel(displayTrigLevel, *this);
}

void DebuggerState::Refresh() {
  if(!uvState.mesh){return;}
  bool checkBoxVal = trigLabelCheckBox->GetVal();
  if (showTrigLabels != checkBoxVal) {
    showTrigLabels = checkBoxVal;
    if (showTrigLabels) {
      Vec2u texSize = trigLabelImage.GetSize();
      MakePerPixelUV(uvState.trigLabels, *uvState.mesh, texSize[0] / 4);
      ui->SetMeshTex(meshId, trigLabelImage, 4);
      //LevelSliderChanged();
    } else {
      uvState.mesh->uv = uv;
      ui->SetMeshTex(meshId, texImage, 4);
    }
    ui->SetMeshNeedsUpdate(meshId);
  }
  int sliderVal = levelSlider->GetVal();
  if (displayTrigLevel != sliderVal) {
    LevelSliderChanged();
  }
}

DebuggerState debState;

void ShowTrigsUpToLevel(unsigned maxlevel, DebuggerState& debState) {
  if (debState.showTrigLabels) {
    auto& image = debState.trigLabelImage;
    Vec2u size = image.GetSize();
    unsigned width = size[0] / 4;
    unsigned lastIdx = width * size[1] - 1;
    auto& mesh = *(debState.uvState.mesh);
    unsigned numTrig = mesh.GetNumTrigs();
    float pixSize = 1.0f / size[0];
    for (size_t t = 0; t < numTrig; t++) {
      unsigned label = debState.uvState.trigLabels[t];
      unsigned tlevel = debState.uvState.trigLevel[t];
      if (tlevel > maxlevel) {
        label = lastIdx;
      }
      unsigned row = label / width;
      unsigned col = label % width;
      Vec2f uv = pixSize * Vec2f(col + 0.5f, row + 0.4f);
      mesh.uv[3 * t] = uv;
      mesh.uv[3 * t + 1] = uv;
      mesh.uv[3 * t + 2] = uv;
    }
    debState.ui->SetMeshNeedsUpdate(debState.meshId);
  }
}

void HandleKeys(KeyboardInput input) {
  std::vector<std::string> keys = input.splitKeys();
  for (const auto& key : keys) {
    bool sliderChange = false;
    int sliderVal = 0;
    if (key == "LeftArrow") {
      sliderChange = true;
      sliderVal = debState.levelSlider->GetVal();
      sliderVal--;
    } else if (key == "RightArrow") {
      sliderVal = debState.levelSlider->GetVal();
      sliderVal++;
      sliderChange = true;
    }

    if (sliderChange) {
      debState.levelSlider->SetVal(sliderVal);
      debState.LevelSliderChanged();
    }
  }
}

void ConvertChan3To4(const Array2D8u& rgb, Array2D8u& rgba) {
  Vec2u inSize = rgb.GetSize();
  unsigned realWidth = inSize[0] / 3;
  rgba.Allocate(4 * realWidth, inSize[1]);
  size_t numPix = realWidth * inSize[1];
  const auto& inData = rgb.GetData();
  auto& outData = rgba.GetData();
  for (size_t i = 0; i < numPix; i++) {
    outData[4 * i] = inData[3 * i];
    outData[4 * i + 1] = inData[3 * i + 1];
    outData[4 * i + 2] = inData[3 * i + 2];
    outData[4 * i + 3] = 255;
  }
}

void DebugUV(UILib & ui) {
  auto mesh = std::make_shared<TrigMesh>();
  mesh->LoadObj("F:/dump/uv_out.obj");
  int meshId = ui.AddMesh(mesh);
  ui.SetMeshColor(meshId, Vec3b(250, 180, 128));
  debState.SetMesh(mesh.get());
  LoadUVSeg("F:/dump/seg.txt", debState.uvState);

  Array2D8u texImage;
  LoadPngColor("F:/dump/checker.png", texImage);
  ConvertChan3To4(texImage, debState.texImage);
  ui.SetMeshTex(meshId, debState.texImage, 4);
  debState.meshId = meshId;
}

//info for voxelizing
//multi-material interface connector designs
struct ConnectorVox {
  void LoadMeshes();
  void VoxelizeMeshes();
  void Log(const std::string& str) const {
    if (LogCb) {
      LogCb(str);
    } else {
      std::cout << str << "\n";
    }
  }
  void Refresh(UILib& ui);

  std::function<void(const std::string&)> LogCb;

  std::vector<std::string> filenames;
  std::string dir;
  bool _hasFileNames = false;
  bool _fileLoaded = false;  
  bool _voxelized = false;
  int _voxResId = -1;
  int _outPrefixId = -1;
  std::vector<TrigMesh> meshes;
  Array3D8u grid;

  UIConf conf;
};

std::string getFileExtension(const std::string& filename) {
  size_t dotPosition = filename.find_last_of('.');
  if (dotPosition != std::string::npos &&
      dotPosition != filename.length() - 1) {
    return filename.substr(dotPosition + 1);
  }
  return "";
}

void ConnectorVox::LoadMeshes() {
  if (filenames.size() == 0) {
    return;
  }
  meshes.resize(filenames.size());
  Log("load meshes");
  for (size_t i = 0; i < filenames.size();i++) {
    const auto& file = filenames[i]; 
    std::string ext = getFileExtension(file);
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

  for (unsigned i = 0; i < meshes.size(); i++) {
    VoxelizeMesh(uint8_t(i + 1), box, voxRes, meshes[i], grid); 
  }
  _voxelized = true;
  for (unsigned i = 0; i < meshes.size(); i++) {
    uint8_t mat = i + 1;
    SaveVolAsObjMesh(
        dir + "/" + conf.outputFile + std::to_string(i + 1) + ".obj", grid,
                     (float*)(&vconf.unit), i + 1);
  }
  std::string gridFilename = dir + "/" + conf.outputFile;
  SaveVoxTxt(grid, voxRes, gridFilename);
  Log("voxelize meshes done");
}

bool FloatEq(float a, float b, float eps) { return std::abs(a - b) < eps; }

void ConnectorVox::Refresh(UILib & ui) {
  //update configs
  bool confChanged = false;
  const float EPS = 5e-5f;
  const float umToMM=0.001f;
  if (_voxResId >= 0) {
    std::shared_ptr<InputInt> input =
        std::dynamic_pointer_cast<InputInt>(ui.GetWidget(_voxResId));
    float uiVoxRes = conf.voxResMM;
    if (input &&input->_entered) {
      uiVoxRes = float(input->_value * umToMM);      
      input->_entered = false;
    }
    if (!FloatEq(uiVoxRes, conf.voxResMM,EPS)) {
      conf.voxResMM = uiVoxRes;
      confChanged = true;
    }
  }
  if (_outPrefixId >= 0) {
    std::shared_ptr<InputText> input =
        std::dynamic_pointer_cast<InputText>(ui.GetWidget(_outPrefixId));
    if (input && input->_entered) {
      input->_entered = false;
    }
    conf.outputFile = input->GetString();
    confChanged = true;
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

ConnectorVox connector;

void OnOpenConnectorMeshes(const std::vector<std::string>& files) {
  connector.filenames = files;
  connector._fileLoaded = false;
  connector._hasFileNames = true;
}

void OpenConnectorMeshes(UILib& ui, const UIConf & conf) {
  ui.SetMultipleFilesOpenCb(OnOpenConnectorMeshes);
  ui.ShowFileOpen(true, conf.workingDir);
}

void OnChangeDir(std::string dir, UIConf& conf) {
  conf.workingDir = dir;
  conf.Save();
}

void LogToUI(const std::string & str, UILib&ui, int statusLabel) {
  ui.SetLabelText(statusLabel, str);
}

struct PcdPoints {
  std::vector<float> _data;
  //3 floats for xyz, 4 for xyz then RGB
  unsigned floatsPerPoint = 3;
  void Allocate(unsigned numPoints) {
    _data.resize(numPoints * floatsPerPoint);
  }
  size_t NumPoints() const { return _data.size() / floatsPerPoint; }
  size_t NumBytes() const {return _data.size() * sizeof(float);}
  float* data() { return _data.data(); }
  const float* data() const{ return _data.data(); }

  float* operator[](size_t i) { return &(_data[i * floatsPerPoint]);}
  const float* operator[](size_t i) const{ return &(_data[i * floatsPerPoint]); }
};

std::string get_suffix(const std::string& str,
                       const std::string& delimiters = ".") {
  size_t pos = str.find_last_of(delimiters);
  if (pos == std::string::npos) {
    return "";  // No suffix found
  }
  return str.substr(pos + 1);
}

void LoadPCD(const std::string& file, PcdPoints& points) {
  std::cout << "loading " << file << "\n";
  std::ifstream in(file, std::ios_base::binary);
  std::string line;
  size_t numPoints = 0;
  std::string dataType = "binary";
  while (std::getline(in, line)) {
    if (line.size() == 0) {
      continue;
    }
    if (line[0] == '#') {
      continue;
    }
    std::istringstream iss(line);
    std::string tok;
    iss >> tok;
    if (tok == "DATA") {
      iss >> dataType;      
      break;
    }
    if (tok == "POINTS") {
      iss >> numPoints;      
    } else if (tok == "SIZE") {
      unsigned count = 0;
      unsigned bytes = 0;
      while (iss >> bytes) {
        count++;
      }
      points.floatsPerPoint = count;
    }
    continue;
  }
  if (dataType == "binary") {
    points.Allocate(points.floatsPerPoint * numPoints);
    in.read((char*)points.data(), points.NumBytes());
  } else {
    std::cout<<"unimplemented data type "<<dataType<<"\n";
  }
}

void ComputeBBox(BBox & box, const PcdPoints& points) {
  if (points.NumPoints() == 0) {
    return;
  }
  
  Vec3f * pt = (Vec3f*)points[0] ;
  box.vmin = *pt;
  box.vmax = *pt;
  for (size_t i = 1; i < points.NumPoints(); i++) {
    pt = (Vec3f*)(points[i]);
    for (unsigned d = 0; d < 3; d++) {
      box.vmin[d] = std::min((*pt)[d], box.vmin[d]);
      box.vmax[d] = std::max((*pt)[d], box.vmax[d]);
    }
  }
}

std::string remove_suffix(const std::string& str, const std::string& suffix) {
  size_t pos = str.rfind(suffix);
  if (pos != std::string::npos) {
    return str.substr(0, pos);
  } else {
    return str;  // Suffix not found, return original string
  }
}

void RGBToGrey(uint32_t& c) {
  uint8_t red = (c >> 16) & 0xff;
  uint8_t green = (c >> 8) & 0xff;
  uint8_t blue = (c) & 0xff;
  float grey = 0.299 * red + 0.587 * green + 0.114 * blue;
  if (grey > 255) {
    grey = 255;
  }
  c=uint32_t(grey);
}

void PreviewPCDToPng(std::string pcdfile) { 
  PcdPoints points;
  LoadPCD(pcdfile, points); 
  BBox box;
  ComputeBBox(box,points);
  std::cout << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2] << "\n";
  std::cout << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2] << "\n";

  Vec3f at = 0.5f * (box.vmin + box.vmax);
  Vec3f size = box.vmax = box.vmin;
  float diagLen = size.norm();
  float eyeDist = 2 * diagLen;
  Vec3f eye = box.vmax + eyeDist * (box.vmax-box.vmin);

  const unsigned IMAGE_WIDTH = 800;
  Array2D8u image(IMAGE_WIDTH, IMAGE_WIDTH);
  std::filesystem::path p(pcdfile);
  std::string parent = p.parent_path().string();
  std::string filename = p.filename().string();
  std::string name = remove_suffix(filename, ".");
  std::string imageFile = parent + "/" + name + ".png";
  
  image.Fill(0);

  Vec3f up(0, 1, 0);

  Vec3f dir = at-eye;
  dir.normalize();
  Vec3f xAxis = up.cross(dir);
  xAxis.normalize();
  up = dir.cross(xAxis);

  Vec2u imageSize = image.GetSize();
  float aspect = imageSize[0] / (float)imageSize[1];
  float fov = 0.785;
  float xLen = eyeDist * std::tan(fov);
  bool hasColor = (points.floatsPerPoint == 4);
  unsigned drawn = 0;
  for (size_t i = 0; i < points.NumPoints(); i++) {
    float * ptr = points[i];
    Vec3f pos = *(Vec3f*)ptr;
    uint32_t color = 128;
    if (hasColor) {
      color = *(uint32_t*)(ptr+3);
      RGBToGrey(color);
    }
    Vec3f d = pos - eye;
    float dist = d.dot(dir);
    if (dist < 0.0001f) {
      continue;
    }
    if (!hasColor) {
      color = 255 - uint8_t(std::min(255.0f, 128.0f * dist / eyeDist));
    }
    float u = (1.0f / dist) * d.dot(xAxis);
    float v = (1.0f / dist) * d.dot(up);
    unsigned col = imageSize[0] / 2 + imageSize[0] * u / xLen;
    unsigned row = imageSize[1] / 2 + imageSize[1] * v / xLen;
    if (col < imageSize[0] && row < imageSize[1]) {
      image(col, row) = uint8_t(color);
      drawn++;
    }
  }
  std::cout << "draw "<<drawn<<" out of " << points.NumPoints() << "\n";
  SavePngGrey(imageFile, image);
}

int pcdDirId = -1;

void ProcessPCDDir(const std::string & dir) {
  try {
    for (auto it = std::filesystem::directory_iterator(dir);
         it != std::filesystem::directory_iterator{}; ++it) {
      if (it->is_regular_file()) {
        std::string pcdfile = it->path().string();
        std::string suffix = get_suffix(pcdfile);
        std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                       ::tolower);
        if (suffix != "pcd") {
          continue;
        }
        PreviewPCDToPng(pcdfile);
      } else {
        ProcessPCDDir(it->path().string());
      }
    }
  } catch (std::exception e) {
    std::cout << e.what() << "\n";
  }
}

void TestPCD() {
  std::string dir =
      "H:/packing/cardinal_bin_packing_"
      "dataset/partial_gocarts_dataset";
  ProcessPCDDir(dir);
}

int main(int, char**) {
  //TestNanoVdb();
  UILib ui;
  connector.conf.Load("");
  ui.SetFontsDir("./fonts");
  std::function<void()> btFunc = std::bind(&TestImageDisplay, std::ref(ui));
  ui.SetWindowSize(1280, 800);
  ui.AddButton("Preview Pcd", [&] {
    std::shared_ptr<InputText> input =
        std::dynamic_pointer_cast<InputText>(ui.GetWidget(pcdDirId));
    
    ProcessPCDDir(input->GetString());
  });
  pcdDirId = ui.AddWidget(
      std::make_shared<InputText>("pcd dir", connector.conf.workingDir));
  ui.AddButton("Open Meshes", [&] { OpenConnectorMeshes(ui, connector.conf); });
  int buttonId = ui.AddButton("GLInfo", {});
  int gl_info_id = ui.AddLabel(" ");
  std::function<void()> showGLInfoFunc =
      std::bind(&ShowGLInfo, std::ref(ui), gl_info_id);
  ui.SetButtonCallback(buttonId, showGLInfoFunc);
  ui.AddButton("Show image", btFunc);
  ui.SetWindowSize(1280, 800);
  ui.SetChangeDirCb([&](const std::string& dir) { OnChangeDir(dir, connector.conf); });
  debState.sliderId = ui.AddSlideri("trig level", 20, 0, 9999);
  debState.levelSlider =
      std::dynamic_pointer_cast<Slideri>(ui.GetWidget(debState.sliderId));
  int boxId = ui.AddCheckBox("Show Trig Label", false);
  debState.trigLabelCheckBox =
      std::dynamic_pointer_cast<CheckBox>(ui.GetWidget(boxId));
  ui.SetKeyboardCb(HandleKeys);
  int statusLabel = ui.AddLabel("status");
  connector.LogCb =
      std::bind(LogToUI, std::placeholders::_1, std::ref(ui), statusLabel);
  connector._voxResId = ui.AddWidget(std::make_shared<InputInt>("vox res um", 32));
  connector._outPrefixId =
      ui.AddWidget(std::make_shared<InputText> ("output file", connector.conf.outputFile));
  ui.Run();
  
  const unsigned PollFreqMs = 20;
  debState.ui = &ui;
  while (ui.IsRunning()) {
    debState.Refresh();
    connector.Refresh(ui);
    std::this_thread::sleep_for(std::chrono::milliseconds(PollFreqMs));
  }
  connector.conf.Save();
  return 0;
}
