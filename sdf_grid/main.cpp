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
  std::string confDir;
  float baseVal = 0.5f;
  float multiplier = 0.1f;
  float minVal = 0.1;
  float maxVal = 1;
  float minDist = -1000;
  float maxDist = 2;
};

struct GridConf{
  std::string boundMesh;
  float backgroundVal = 1.5f;
  std::vector<MeshConf> meshConfs;

};

GridConf ReadConfig(const std::string &filename) {
  std::ifstream configFile(filename);
  GridConf conf;
  if (!configFile.is_open()) {
    std::cout << "can not open json config file " << filename << "\n";
    return conf;
  }
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
  conf.backgroundVal = json["backgroundVal"].getFloat();
  return conf;
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cout << "Usage: sdfgrid config.json\n";
    return 0;
  }
  std::string filename(argv[1]);
  GridConf conf = ReadConfig(filename);
  return 0;
}
