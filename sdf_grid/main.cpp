#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>

#include "TrigMesh.h"
#include "BBox.h"
#include "json.h"

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
  // inferred from file name
  std::string dir;

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

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "Usage: sdfgrid config.json\n";
    return 0;
  }
  std::string filename(argv[1]);
  GridConf conf = ReadConfig(filename);
  std::cout <<"using config:\n"<< conf.toString() << "\n";
  return 0;
}
