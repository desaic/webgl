#include "PackingConfig.h"

#include <cstdlib>
#include <filesystem>
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

void PackingConfig::ParseArgs(int argc, char **argv) {
  if (argc > 1 && argv[1] != nullptr && argv[1][0] != '-') {
    dataDir = argv[1];
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
      << " broadPhaseDx " << broadPhaseDx
      << " subgridCellSize " << subgridCellSize << "\n";
  oss << "maxTrialCount " << maxTrialCount << " startStep " << startStep
      << " startItem " << startItem << "\n";
  oss << "resume " << resume;
  if (resume) {
    oss << " " << ResumePackPath();
  }
  oss << "\n";
  oss << "trajSaveInterval " << trajSaveInterval << " packSaveInterval "
      << packSaveInterval << "\n";
  if (maxSecondsPerStep > 0.0f) {
    oss << "maxSecondsPerStep " << maxSecondsPerStep << "\n";
  }
  return oss.str();
}
