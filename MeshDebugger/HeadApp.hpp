#pragma once
#include "UILib.h"
#include "Array3D.h"
#include "UIConf.h"
//info for voxelizing
//multi-material interface connector designs
class HeadApp {
public:
  void Init(UILib* ui);
 void Init3DScene(UILib* ui); 
  void LoadMeshes();
  void Log(const std::string& str) const;
  void Refresh();
  void OnOpenMeshes(const std::vector<std::string>& files);
  void OpenMeshes(UILib& ui, const UIConf& conf);
  std::function<void(const std::string&)> LogCb;

  std::vector<std::string> filenames;
  std::string dir;
  bool _hasFileNames = false;
  bool _fileLoaded = false;  
  bool _processed = false;
  // approximate size of meshes in mm.
  const float ApproxSize = 200;
  int _outPrefixId = -1;
  int _statusLabel = -1;
  int _fpsLabel = -1;
  std::shared_ptr<TrigMesh> _floor;
  int _floorInst = -1;
  std::vector<std::shared_ptr<TrigMesh>> _meshes;
  std::vector<int> _instanceIds;

  Array2D8u _floorColor;
  Array3D8u grid;
  UILib * _ui = nullptr;
  std::shared_ptr<InputInt> _resInput;
  std::shared_ptr<InputInt> _waxGapInput;
  UIConf conf;
};
