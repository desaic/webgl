#pragma once
#include "UILib.h"
#include "Array3D.h"
#include "UIConf.h"
//info for voxelizing
//multi-material interface connector designs
class InflateApp {
 public:
  void Init(UILib* ui);
  void InflateMeshes();
  void LoadMeshes();
  void Log(const std::string& str) const;
  void Refresh();
  void OnOpenMeshes(const std::vector<std::string>& files);
  void OpenMeshes(UILib& ui, const UIConf& conf);

 private:
  std::function<void(const std::string&)> LogCb;

  std::vector<std::string> filenames;
  std::string dir;
  std::string filePrefix;
  bool _hasFileNames = false;
  bool _fileLoaded = false;  
  bool _inflated = false;
  int _voxResId = -1;
  int _thicknessId = -1;
  int _outPrefixId = -1;
  int _statusLabel = -1;
  std::vector<TrigMesh> meshes;
  Array3D8u grid;
  UILib * _ui;
  std::shared_ptr<InputFloat> _resInput;
  std::shared_ptr<InputFloat> _thicknessInput;
  UIConf conf;
};
