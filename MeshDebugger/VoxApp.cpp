
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