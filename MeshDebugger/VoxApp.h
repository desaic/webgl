#pragma once
#include "UILib.h"
#include "Array3D.h"
#include "UIConf.h"
//info for voxelizing
//multi-material interface connector designs
struct ConnectorVox {
  void Init(UILib* ui);
  void LoadMeshes();
  void VoxelizeMeshes();
  void Log(const std::string& str) const;
  void Refresh();
  void OnOpenMeshes(const std::vector<std::string>& files);
  void OpenMeshes(UILib& ui, const UIConf& conf);
  std::function<void(const std::string&)> LogCb;

  std::vector<std::string> filenames;
  std::string dir;
  bool _hasFileNames = false;
  bool _fileLoaded = false;  
  bool _voxelized = false;
  int _voxResId = -1;
  int _waxGapId = -1;
  int _outPrefixId = -1;
  int _statusLabel = -1;
  std::vector<TrigMesh> meshes;
  Array3D8u grid;
  UILib * _ui;
  std::shared_ptr<InputInt> _resInput;
  std::shared_ptr<InputInt> _waxGapInput;
  UIConf conf;
};
