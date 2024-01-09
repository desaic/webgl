#pragma once
#include "UILib.h"
#include "Array3D.h"
#include "UIConf.h"
//info for voxelizing
//multi-material interface connector designs
struct ConnectorVox {
  void LoadMeshes();
  void VoxelizeMeshes();
  void Log(const std::string& str) const;
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
