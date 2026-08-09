#include "PackingConfig.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

static std::string Join(const std::string &dir, const std::string &rel) {
  if (rel.empty()) {
    return dir;
  }
  fs::path p = fs::path(dir) / fs::path(rel);
  return p.string();
}

std::string PackingConfig::MeshDir() const {
  // trailing separator kept. LoadAllMeshInfo and stats paths append to it.
  return Join(dataDir, fruitSubdir) + "/";
}

std::string PackingConfig::ContainerPath() const {
  return Join(dataDir, containerFile);
}

std::string PackingConfig::InnerContainerPath() const {
  return Join(dataDir, innerContainerFile);
}

std::string PackingConfig::OutputFolder() const {
  return Join(dataDir, outSubdir) + "/";
}

std::string PackingConfig::ResumePackPath() const {
  return Join(dataDir, resumePackFile);
}

static bool ParseBool(const std::string &v) {
  return v == "1" || v == "true" || v == "yes" || v == "on";
}

bool PackingConfig::LoadFromFile(const std::string &path) {
  std::ifstream in(path);
  if (!in.good()) {
    std::cout << "could not open config " << path << "\n";
    return false;
  }
  std::string line;
  unsigned lineNum = 0, unknown = 0;
  while (std::getline(in, line)) {
    lineNum++;
    // a '#' only starts a comment at the start of a line or after
    // whitespace, so a directory name may contain one.
    for (size_t i = 0; i < line.size(); i++) {
      if (line[i] != '#') {
        continue;
      }
      if (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t') {
        line = line.substr(0, i);
        break;
      }
    }
    std::istringstream ls(line);
    std::string key;
    if (!(ls >> key)) {
      continue;
    }
    // the rest of the line, so a path may contain spaces.
    std::string value;
    std::getline(ls, value);
    size_t first = value.find_first_not_of(" \t\r");
    size_t last = value.find_last_not_of(" \t\r");
    value = (first == std::string::npos) ? "" : value.substr(first, last - first + 1);

    if (key == "dataDir") {
      dataDir = value;
    } else if (key == "fruitSubdir") {
      fruitSubdir = value;
    } else if (key == "containerFile") {
      containerFile = value;
    } else if (key == "innerContainerFile") {
      innerContainerFile = value;
    } else if (key == "outSubdir") {
      outSubdir = value;
    } else if (key == "dx") {
      dx = float(std::atof(value.c_str()));
    } else if (key == "containerSDFDx") {
      containerSDFDx = float(std::atof(value.c_str()));
    } else if (key == "broadPhaseDx") {
      broadPhaseDx = float(std::atof(value.c_str()));
    } else if (key == "gridDx") {
      gridDx = float(std::atof(value.c_str()));
    } else if (key == "subgridCellSize") {
      subgridCellSize = float(std::atof(value.c_str()));
    } else if (key == "maxTrialCount") {
      maxTrialCount = unsigned(std::atoi(value.c_str()));
    } else if (key == "startStep") {
      startStep = unsigned(std::atoi(value.c_str()));
    } else if (key == "startItem") {
      startItem = unsigned(std::atoi(value.c_str()));
    } else if (key == "resumePackFile") {
      resumePackFile = value;
    } else if (key == "resume") {
      resume = ParseBool(value);
    } else if (key == "trajSaveInterval") {
      trajSaveInterval = unsigned(std::atoi(value.c_str()));
    } else if (key == "packSaveInterval") {
      packSaveInterval = unsigned(std::atoi(value.c_str()));
    } else if (key == "computeStats") {
      computeStats = ParseBool(value);
    } else if (key == "maxSecondsPerStep") {
      maxSecondsPerStep = float(std::atof(value.c_str()));
    } else {
      std::cout << path << ":" << lineNum << " unknown key " << key << "\n";
      unknown++;
    }
  }
  std::cout << "config " << path;
  if (unknown > 0) {
    std::cout << " (" << unknown << " unknown keys skipped)";
  }
  std::cout << "\n";
  return true;
}

bool PackingConfig::ClampStartStep(size_t numSteps) {
  if (numSteps == 0) {
    return false;
  }
  unsigned last = unsigned(numSteps - 1);
  if (startStep > last) {
    std::cout << "startStep " << startStep << " past the last step " << last
              << ", clamped\n";
    startStep = last;
    return true;
  }
  return false;
}

void PackingConfig::ParseArgs(int argc, char **argv) {
  bool loaded = false;
  for (int i = 1; i + 1 < argc; i++) {
    if (argv[i] != nullptr && std::string(argv[i]) == "--config") {
      loaded = LoadFromFile(argv[i + 1]);
    }
  }
  if (!loaded && argc > 1 && argv[1] != nullptr && argv[1][0] != '-') {
    // a file is a config, a directory is a data root. no flag needed for
    // either, since those are the only two things anyone passes here.
    std::error_code ec;
    if (fs::is_regular_file(argv[1], ec)) {
      LoadFromFile(argv[1]);
    } else {
      dataDir = argv[1];
    }
    return;
  }
  if (loaded) {
    return;
  }
  const char *env = std::getenv("FRUIT_HAND_DIR");
  if (env != nullptr && env[0] != 0) {
    dataDir = env;
  }
}

std::string PackingConfig::toString() const {
  std::ostringstream oss;
  oss << "dataDir " << dataDir << "\n";
  oss << "meshDir " << MeshDir() << "\n";
  oss << "container " << ContainerPath() << "\n";
  oss << "innerContainer " << InnerContainerPath() << "\n";
  oss << "output " << OutputFolder() << "\n";
  oss << "dx " << dx << " containerSDFDx " << containerSDFDx
      << " broadPhaseDx " << broadPhaseDx << " gridDx " << gridDx
      << " subgridCellSize " << subgridCellSize << "\n";
  oss << "maxTrialCount " << maxTrialCount << " startStep " << startStep
      << " startItem " << startItem << "\n";
  oss << "resume " << resume;
  if (resume) {
    oss << " " << ResumePackPath();
  }
  oss << "\n";
  oss << "trajSaveInterval " << trajSaveInterval << " packSaveInterval "
      << packSaveInterval << " computeStats " << computeStats << "\n";
  if (maxSecondsPerStep > 0.0f) {
    oss << "maxSecondsPerStep " << maxSecondsPerStep << "\n";
  }
  return oss.str();
}
