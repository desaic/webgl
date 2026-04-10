#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "AdapSDF.h"
#include "AdapUDF.h"
#include "SDFMesh.h"
#include "TrigMesh.h"
#include "BBox.h"
#include "BBox.h"
#include "json.h"
#include "Array2D.h"
#include "Array3D.h"
#include "Array3DUtils.h"
#include "AdapSDF.h"
#include "FastSweep.h"
#include "VoxIO.h"
#include "ImageIO.h"

namespace fs = std::filesystem;

struct MeshConf{
  std::string name;
  float baseVal = 0.5f;
  float multiplier = 0.1f;
  float minVal = 0.1;
  float maxVal = 1;
  float minDist = -1000;
  float maxDist = 2;
  
  std::string toString()const {
    std::ostringstream oss;
    oss << "name = " << name<<"\n";
    oss << "baseVal = " << baseVal << "\n";
    oss << "multiplier = " << multiplier << "\n";
    oss << "minVal = " << minVal << "\n";
    oss << "maxVal = " << maxVal << "\n";
    oss << "minDist = " << minDist << "\n";
    oss << "maxDist = " << maxDist << "\n";
    return oss.str();
  }
  
};

struct GridConf{
  std::string boundMesh;
  float backgroundVal = 1.5f;
  float dx = 1.0f;
  // inferred from file name
  std::string dir;
  std::string output = "grid.txt";
  std::vector<MeshConf> meshConfs;

  std::string toString()const;
};

std::string GridConf::toString() const{
  std::ostringstream oss;
  oss << "boundMesh = " << boundMesh << "\n";
  oss << "backgroundVal = " << backgroundVal << "\n";
  oss << "dir = " << dir << "\n";
  for (const auto& m : meshConfs) {
    oss << m.toString() << "\n";
  }
  return oss.str();
}

GridConf ReadConfig(const std::string &filename) {
  std::ifstream configFile(filename);
  GridConf conf;
  if (!configFile.is_open()) {
    std::cout << "can not open json config file " << filename << "\n";
    return conf;
  }

  fs::path filePath(filename);
  fs::path fullDirPath = filePath.parent_path();
  conf.dir = fullDirPath.string();

  std::string content((std::istreambuf_iterator<char>(configFile)),
    std::istreambuf_iterator<char>());
  auto [status, json] = jt::Json::parse(content);
  if (status != jt::Json::success) {
    std::cout << filename << " is not a valid json.\n";
    return conf;
  }
  if (!json.isObject()) {
    std::cout << filename << " is not a json object.\n";
    return conf;
  }
  conf.boundMesh = json["boundMesh"].getString();
  conf.backgroundVal = float(json["backgroundVal"].getNumber());
  if (json.contains("dx")) {
    conf.dx = json["dx"].getNumber();
  }
  if (json.contains("output")) {
    conf.output = json["output"].getString();
  }
  if (!json["meshes"].isArray()) {
    std::cout<< "config missing meshes array\n";
  }
  else {
    std::vector<jt::Json>& meshes= json["meshes"].getArray();
    for (jt::Json& mesh : meshes) {
      MeshConf meshConf;
      if (!mesh.contains("name")) {
        continue;
      }
      meshConf.name = mesh["name"].getString();
      if (mesh.contains("baseVal")) {
        meshConf.baseVal = float(mesh["baseVal"].getNumber());
      }
      if (mesh.contains("multiplier")) {
        meshConf.multiplier = float(mesh["multiplier"].getNumber());
      }
      if (mesh.contains("minVal")) {
        meshConf.minVal = float(mesh["minVal"].getNumber());
      }
      if (mesh.contains("maxVal")) {
        meshConf.maxVal = float(mesh["maxVal"].getNumber());
      }
      if (mesh.contains("minDist")) {
        meshConf.minDist = float(mesh["minDist"].getNumber());
      }
      if (mesh.contains("maxDist")) {
        meshConf.maxDist = float(mesh["maxDist"].getNumber());
      }

      conf.meshConfs.push_back(meshConf);
    }
  }
  return conf;
}

// only computes coarse distance.
std::shared_ptr<AdapSDF> ComputeSDF(float distUnit, float h, TrigMesh& mesh) {
  std::shared_ptr<AdapSDF> softSdf = std::make_shared<AdapSDF>();
  softSdf->voxSize = h;
  softSdf->band = 16;
  softSdf->distUnit = distUnit;
  softSdf->BuildTrigList(&mesh);
  softSdf->Compress();
  mesh.ComputePseudoNormals();
  softSdf->ComputeCoarseDist();
  CloseExterior(softSdf->dist, softSdf->MAX_DIST);
  Array3D8u frozen;
  softSdf->FastSweepCoarse(frozen);
  return softSdf;
}

std::string getSuffix(const std::string& str) {
  size_t lastDotPos = str.find_last_of('.');
  if (lastDotPos == std::string::npos || lastDotPos == str.length() - 1) {
    return "";
  }
  return str.substr(lastDotPos + 1);
}

void LoadMesh(TrigMesh& mesh, const std::string& name) {
  std::string suffix = getSuffix(name);
  std::transform(suffix.begin(), suffix.end(), suffix.begin(),
    [](unsigned char c) { return std::tolower(c); });
  if (suffix == "stl") {
    mesh.LoadStl(name);
  }
  else if (suffix == "obj") {
    mesh.LoadObj(name);
  }
  else {
    std::cout << "unsupported mesh file format " << suffix << "\n";
  }
}

Array2D8u GetImageSlice(std::shared_ptr<AdapSDF> & sdf, unsigned z, float valScale, float valOffset) {
  Vec3u size = sdf->dist.GetSize();
  z = std::clamp(z, 0u, unsigned(size[2] - 1));
  Array2D8u slice(size[0], size[1]);
  for (unsigned y = 0; y < size[1]; y++) {
    for (unsigned x = 0; x < size[0]; x++) {
      short sd = sdf->dist(x, y, z);
      float d = (sd * sdf->distUnit * valScale) + valOffset;
      if (d < -100) {
        d = -100;
      }
      slice(x, y) = d + 100;
    }
  }
  return slice;
}

Array2D8u GetImageSlice(Array3Df& grid, unsigned z, float valScale, float valOffset) {
  Vec3u size = grid.GetSize();
  z = std::clamp(z, 0u, unsigned(size[2] - 1));
  Array2D8u slice(size[0], size[1]);
  for (unsigned y = 0; y < size[1]; y++) {
    for (unsigned x = 0; x < size[0]; x++) {      
      float d = (grid(x, y, z) * valScale) + valOffset;
      if (d < -100) {
        d = -100;
      }
      slice(x, y) = d + 100;
    }
  }
  return slice;
}

void MakeGrid(const GridConf & conf) {
  TrigMesh boxMesh;
  LoadMesh(boxMesh, conf.dir + "/" + conf.boundMesh);
  Box3f box = ComputeBBox(boxMesh.v);
  Vec3f bSize = box.vmax - box.vmin;
  float dx = conf.dx;
  Vec3u gridSize(unsigned(bSize[0] / dx) + 2, unsigned(bSize[1] / dx) + 2,
    unsigned(bSize[2] / dx) + 2);
  Vec3f origin = box.vmin;
  float bgVal = conf.backgroundVal;
  Array3Df grid(gridSize, bgVal);
  float gridLen = std::max(bSize[0], std::max(bSize[1], bSize[2]));
  float maxDistVal = 3 * gridLen;
  for (const auto meshConf : conf.meshConfs) {
    TrigMesh m;
    std::string meshPath = conf.dir + "/" + meshConf.name;
    std::cout << "processing " << meshConf.name << "\n";
    LoadMesh(m, meshPath);
    if (m.t.empty()) {
      std::cout << "failed to load " << meshConf.name << "\n";
      continue;
    }
    float h = dx;
    float distUnit = h * 0.1f;
    std::shared_ptr<AdapSDF> sdf = ComputeSDF(distUnit, h, m);
    Array3D8u outside = FloodOutside(sdf->dist, 0);

    //make sure all internal values are negative
    for (size_t i = 0; i < outside.GetData().size(); i++) {
      short& d = sdf->dist.GetData()[i];
      if (!outside.GetData()[i] && d > 0) {
        d = -d;
      }
    }
    Array2D8u slice = GetImageSlice(sdf, gridSize[2] / 2, 1, 0);
    std::string debugImage = conf.dir + "/sdf_" + meshConf.name + ".png";
    SavePngGrey(debugImage, slice);

    for (unsigned z = 0; z < gridSize[2]; z++) {
      for (unsigned y = 0; y < gridSize[1]; y++) {
        for (unsigned x = 0; x < gridSize[0]; x++) {
          Vec3f coord = origin + Vec3f(x * dx, y * dx, z * dx);
          float dist = sdf->GetCoarseDist(coord);
          dist = std::clamp(dist, -maxDistVal, maxDistVal);
          float val = bgVal;
          // do not write out of range values
          if (dist<meshConf.minDist || dist > meshConf.maxDist) {
            continue;
          }
          val = dist * meshConf.multiplier + meshConf.baseVal;
          val = std::clamp(val, meshConf.minVal, meshConf.maxVal);
          grid(x, y, z) = val;
        }
      }
    }
  }
  float maxVal = 0.001f;
  for (auto f : grid.GetData()) {
    maxVal = std::max(maxVal, f);
  }
  float debugScale = 100 / maxVal;
  std::vector<unsigned> zSamples(3);
  zSamples[0] = gridSize[2] / 20;
  zSamples[1] = gridSize[2] / 2;
  zSamples[2] = gridSize[2] - std::min(gridSize[2], gridSize[2] / 20);
  for (unsigned z : zSamples) {
    Array2D8u slice = GetImageSlice(grid, z, debugScale, 0);
    std::string debugSlicePath = conf.dir + "/debug_slice_" + std::to_string(z) + ".png";
    SavePngGrey(debugSlicePath, slice);
  }
  std::string outFile = conf.dir + "/" + conf.output;
  SaveVoxTxt(grid, Vec3f(dx), outFile, origin);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "Usage: sdfgrid config.json\n";
    return 0;
  }
  std::string filename(argv[1]);
  GridConf conf = ReadConfig(filename);
  std::cout <<"using config:\n"<< conf.toString() << "\n";
  MakeGrid(conf);
  return 0;
}
