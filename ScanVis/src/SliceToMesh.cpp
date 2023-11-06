#include "SliceToMesh.h"
#include "ImageIO.h"

#include <filesystem>

void LoadSlices(std::string dir,
  Array3D8u&vol, Vec3u mn, Vec3u mx) {
  std::vector<std::string> fileList;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    std::string s = entry.path().string();
    fileList.push_back(s);
  }
  size_t numLayers = fileList.size();
  for (size_t i = 0; i < numLayers; i++) {
    std::string file = dir + "/" + std::to_string(i) + ".png";
    Array2D8u img;
    LoadPngGrey(file, img);

  }
}

void slices2Meshes(const ConfigFile& conf)
{
  std::string sliceDir = conf.getString("sliceDir");
  Array3D8u vol;
  Vec3u mn, mx;
  LoadSlices(sliceDir, vol, mn, mx);

}
