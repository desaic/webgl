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

std::string PackingConfig::MediumTargetsPath() const {
  return mediumTargetsFile.empty() ? std::string() : Join(dataDir, mediumTargetsFile);
}

std::string PackingConfig::ExclusionMeshPath() const {
  return Join(dataDir, exclusionMeshFile);
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

    if (key == "snugScore") {
      snugScore = ParseBool(value);
      continue;
    }
    if (key == "snugNudgeSteps") {
      snugNudgeSteps = unsigned(std::atoi(value.c_str()));
      continue;
    }
    if (key == "snugPlan") {
      snugPlan = ParseBool(value);
      continue;
    }
    if (key == "snugPlanNms") {
      snugPlanNms = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugPlanMax") {
      snugPlanMax = unsigned(std::atoi(value.c_str()));
      continue;
    }
    if (key == "snugPlanOverlap") {
      snugPlanOverlap = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugShellCm") {
      snugShellCm = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugMinBuried") {
      snugMinBuried = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugBuriedW") {
      snugBuriedW = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugMinWrap") {
      snugMinWrap = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugWrapW") {
      snugWrapW = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugTouchCm") {
      snugTouchCm = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugRefine") {
      snugRefine = ParseBool(value);
      continue;
    }
    if (key == "snugRefineSteps") {
      snugRefineSteps = unsigned(std::atoi(value.c_str()));
      continue;
    }
    if (key == "snugRefinePasses") {
      snugRefinePasses = unsigned(std::atoi(value.c_str()));
      continue;
    }
    if (key == "snugRefineReach") {
      snugRefineReach = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugRefineMaxPen") {
      snugRefineMaxPen = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugDebugRadius") {
      snugDebugRadius = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugDebugSpot") {
      std::istringstream vs(value);
      float sx = 0.0f, sy = 0.0f, sz = 0.0f;
      vs >> sx >> sy >> sz;
      snugDebugX = sx;
      snugDebugY = sy;
      snugDebugZ = sz;
      continue;
    }
    if (key == "snugPostOverlap") {
      snugPostOverlap = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugRotations") {
      snugRotations = unsigned(std::atoi(value.c_str()));
      continue;
    }
    if (key == "snugWallW") {
      snugWallW = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugWallCap") {
      snugWallCap = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugPosW") {
      snugPosW = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "snugMinDirs") {
      snugMinDirs = unsigned(std::atoi(value.c_str()));
      continue;
    }
    if (key == "snugMinFace") {
      snugMinFace = unsigned(std::atoi(value.c_str()));
      continue;
    }
    if (key == "cavSnug") {
      cavSnug = unsigned(std::atoi(value.c_str()));
      continue;
    }
    if (key == "cavContacts") {
      cavContacts = unsigned(std::atoi(value.c_str()));
      continue;
    }
    if (key == "cavSnugFill") {
      cavSnugFill = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "cavReach") {
      cavReach = float(std::atof(value.c_str()));
      continue;
    }
    if (key == "cavRelocate") {
      cavRelocate = unsigned(std::atoi(value.c_str()));
      continue;
    }
    if (key == "dataDir") {
      dataDir = value;
    } else if (key == "fruitSubdir") {
      fruitSubdir = value;
    } else if (key == "containerFile") {
      containerFile = value;
    } else if (key == "innerContainerFile") {
      innerContainerFile = value;
    } else if (key == "exclusionMeshFile") {
      exclusionMeshFile = value;
    } else if (key == "mediumTargetsFile") {
      mediumTargetsFile = value;
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
    } else if (key == "maxSecondsPerStep") {
      maxSecondsPerStep = std::atof(value.c_str());
    } else if (key == "startStep") {
      startStep = unsigned(std::atoi(value.c_str()));
    } else if (key == "endStep") {
      endStep = unsigned(std::atoi(value.c_str()));
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
    } else if (key == "sphereCover") {
      sphereCover = ParseBool(value);
    } else if (key == "sphereScales") {
      std::istringstream vs(value);
      sphereScales.clear();
      float s;
      while (vs >> s) sphereScales.push_back(s);
    } else if (key == "sphereOverlapCm") {
      sphereOverlapCm = float(std::atof(value.c_str()));
    } else if (key == "sphereHardFrac") {
      sphereHardFrac = float(std::atof(value.c_str()));
    } else if (key == "sphereContactTol") {
      sphereContactTol = float(std::atof(value.c_str()));
    } else if (key == "sphereCost") {
      sphereCost = float(std::atof(value.c_str()));
    } else if (key == "sphereSizeCost") {
      sphereSizeCost = float(std::atof(value.c_str()));
    } else if (key == "sphereSupportMinDiag") {
      sphereSupportMinDiag = float(std::atof(value.c_str()));
    } else if (key == "sphereFieldVoxel") {
      sphereFieldVoxel = float(std::atof(value.c_str()));
    } else if (key == "sphereFieldRadius") {
      sphereFieldRadius = float(std::atof(value.c_str()));
    } else if (key == "sphereFieldSpacing") {
      sphereFieldSpacing = float(std::atof(value.c_str()));
    } else if (key == "sphereFieldMinClear") {
      sphereFieldMinClear = float(std::atof(value.c_str()));
    } else if (key == "sphereHullFaces") {
      sphereHullFaces = ParseBool(value);
    } else if (key == "hullGap") {
      hullGap = float(std::atof(value.c_str()));
    } else if (key == "hullBridgeGap") {
      hullBridgeGap = float(std::atof(value.c_str()));
    } else if (key == "hullMaxNeighbors") {
      hullMaxNeighbors = unsigned(std::atoi(value.c_str()));
    } else if (key == "hullSamples") {
      hullSamples = unsigned(std::atoi(value.c_str()));
    } else if (key == "hullMinDepth") {
      hullMinDepth = float(std::atof(value.c_str()));
    } else if (key == "hullExclMargin") {
      hullExclMargin = float(std::atof(value.c_str()));
    } else if (key == "hullSurfaceBand") {
      hullSurfaceBand = float(std::atof(value.c_str()));
    } else if (key == "hullPatchSize") {
      hullPatchSize = float(std::atof(value.c_str()));
    } else if (key == "hullSupportDist") {
      hullSupportDist = float(std::atof(value.c_str()));
    } else if (key == "sphereMouths") {
      sphereMouths = ParseBool(value);
    } else if (key == "mouthNeighborDist") {
      mouthNeighborDist = float(std::atof(value.c_str()));
    } else if (key == "mouthMaxNormalDot") {
      mouthMaxNormalDot = float(std::atof(value.c_str()));
    } else if (key == "mouthOuterBand") {
      mouthOuterBand = float(std::atof(value.c_str()));
    } else if (key == "mouthCell") {
      mouthCell = float(std::atof(value.c_str()));
    } else if (key == "mouthTargetWeight") {
      mouthTargetWeight = float(std::atof(value.c_str()));
    } else if (key == "mouthGainWeight") {
      mouthGainWeight = float(std::atof(value.c_str()));
    } else if (key == "mouthPlacementLimit") {
      mouthPlacementLimit = unsigned(std::atoi(value.c_str()));
    } else if (key == "sphereFrontRequireMouth") {
      sphereFrontRequireMouth = std::atoi(value.c_str()) != 0;
    } else if (key == "sphereAuditOpenMouths") {
      sphereAuditOpenMouths = std::atoi(value.c_str()) != 0;
    } else if (key == "sphereMouthRepairPasses") {
      sphereMouthRepairPasses = unsigned(std::atoi(value.c_str()));
    } else if (key == "shellVoxel") {
      shellVoxel = float(std::atof(value.c_str()));
    } else if (key == "shellOutset") {
      shellOutset = float(std::atof(value.c_str()));
    } else if (key == "shellError") {
      shellError = float(std::atof(value.c_str()));
    } else if (key == "shellPasses") {
      shellPasses = ParseBool(value);
    } else if (key == "fillMinDiag") {
      fillMinDiag = float(std::atof(value.c_str()));
    } else if (key == "fillMaxDiag") {
      fillMaxDiag = float(std::atof(value.c_str()));
    } else if (key == "shellSampleSpacing") {
      shellSampleSpacing = float(std::atof(value.c_str()));
    } else if (key == "crevDepth") {
      crevDepth = float(std::atof(value.c_str()));
    } else if (key == "crevMinArea") {
      crevMinArea = float(std::atof(value.c_str()));
    } else if (key == "crevGroupMax") {
      crevGroupMax = unsigned(std::atoi(value.c_str()));
    } else if (key == "oldWayMedium") {
      oldWayMedium = ParseBool(value);
    } else if (key == "oldWayOverlap") {
      oldWayOverlap = ParseBool(value);
    } else if (key == "graphGap") {
      graphGap = float(std::atof(value.c_str()));
    } else if (key == "forceKind") {
      forceKind = value;
    } else if (key == "forceGridDx") {
      forceGridDx = float(std::atof(value.c_str()));
    } else if (key == "forceGridMargin") {
      forceGridMargin = float(std::atof(value.c_str()));
    } else if (key == "vhacdMaxHulls") {
      vhacdMaxHulls = unsigned(std::atoi(value.c_str()));
    } else if (key == "vhacdResolution") {
      vhacdResolution = unsigned(std::atoi(value.c_str()));
    } else if (key == "vhacdErrorPct") {
      vhacdErrorPct = float(std::atof(value.c_str()));
    } else if (key == "vhacdFill") {
      vhacdFill = value;
    } else if (key == "vhacdOnly") {
      vhacdOnly = value;
    } else if (key == "vhacdDepth") {
      vhacdDepth = unsigned(std::atoi(value.c_str()));
    } else if (key == "vhacdMaxVerts") {
      vhacdMaxVerts = unsigned(std::atoi(value.c_str()));
    } else if (key == "hullDir") {
      hullDir = value;
    } else if (key == "hullInset") {
      hullInset = float(std::atof(value.c_str()));
    } else if (key == "hullSampleSpacing") {
      hullSampleSpacing = float(std::atof(value.c_str()));
    } else if (key == "forceExclusionSkip") {
      forceExclusionSkip = float(std::atof(value.c_str()));
    } else if (key == "cavDx") {
      cavDx = float(std::atof(value.c_str()));
    } else if (key == "cavBand") {
      cavBand = float(std::atof(value.c_str()));
    } else if (key == "cavR") {
      cavR = float(std::atof(value.c_str()));
    } else if (key == "cavMinClear") {
      cavMinClear = float(std::atof(value.c_str()));
    } else if (key == "cavPasses") {
      cavPasses = unsigned(std::atoi(value.c_str()));
    } else if (key == "cavTrials") {
      cavTrials = unsigned(std::atoi(value.c_str()));
    } else if (key == "cavMinGain") {
      cavMinGain = float(std::atof(value.c_str()));
    } else if (key == "cavMaxPerPass") {
      cavMaxPerPass = unsigned(std::atoi(value.c_str()));
    } else if (key == "cavCone") {
      cavCone = float(std::atof(value.c_str()));
    } else if (key == "cavObjClear") {
      cavObjClear = float(std::atof(value.c_str()));
    } else if (key == "cavWallTol") {
      cavWallTol = float(std::atof(value.c_str()));
    } else if (key == "cavCap") {
      cavCap = unsigned(std::atoi(value.c_str()));
    } else if (key == "cavCapOnly") {
      cavCapOnly = unsigned(std::atoi(value.c_str()));
    } else if (key == "cavDimple") {
      cavDimple = unsigned(std::atoi(value.c_str()));
    } else if (key == "cavDimpleOnly") {
      cavDimpleOnly = unsigned(std::atoi(value.c_str()));
    } else if (key == "cavFillMin") {
      cavFillMin = float(std::atof(value.c_str()));
    } else if (key == "cavFillVol") {
      cavFillVol = float(std::atof(value.c_str()));
    } else if (key == "cavProtrude") {
      cavProtrude = float(std::atof(value.c_str()));
    } else if (key == "cavDimpleClear") {
      cavDimpleClear = float(std::atof(value.c_str()));
    } else if (key == "forcePasses") {
      forcePasses = unsigned(std::atoi(value.c_str()));
    } else if (key == "oldWayKinds") {
      oldWayKinds = value;
    } else if (key == "oldWayInner") {
      oldWayInner = ParseBool(value);
    } else if (key == "clearanceGate") {
      clearanceGate = ParseBool(value);
    } else if (key == "oldWayGroup") {
      oldWayGroup = unsigned(std::atoi(value.c_str()));
    } else if (key == "oldWayErode") {
      oldWayErode = unsigned(std::atoi(value.c_str()));
    } else if (key == "shellEvalOnly") {
      shellEvalOnly = ParseBool(value);
    } else if (key == "shellSwap") {
      shellSwap = ParseBool(value);
    } else if (key == "swapTrials") {
      swapTrials = unsigned(std::atoi(value.c_str()));
    } else if (key == "swapForceW") {
      swapForceW = float(std::atof(value.c_str()));
    } else if (key == "swapMaxOverlap") {
      swapMaxOverlap = float(std::atof(value.c_str()));
    } else if (key == "shellFruitMargin") {
      shellFruitMargin = float(std::atof(value.c_str()));
    } else if (key == "holeMinArea") {
      holeMinArea = float(std::atof(value.c_str()));
    } else if (key == "patchRadius") {
      patchRadius = float(std::atof(value.c_str()));
    } else if (key == "patchAngle") {
      patchAngle = float(std::atof(value.c_str()));
    } else if (key == "shellFromVoxels") {
      shellFromVoxels = ParseBool(value);
    } else if (key == "shellFraction") {
      shellFraction = float(std::atof(value.c_str()));
    } else if (key == "sphereMaxX") {
      sphereMaxX = float(std::atof(value.c_str()));
    } else if (key == "sphereProbe") {
      std::istringstream vs(value);
      float x, y, z;
      if (vs >> x >> y >> z) {
        sphereProbes.push_back(x);
        sphereProbes.push_back(y);
        sphereProbes.push_back(z);
      }
    } else if (key == "sphereMaxCount") {
      sphereMaxCount = unsigned(std::atoi(value.c_str()));
    } else if (key == "computeStats") {
      computeStats = ParseBool(value);
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
  // startStep == numSteps is valid: skip every plan step, go straight to
  // the crevice phase on a resumed pack.
  unsigned limit = unsigned(numSteps);
  if (startStep > limit) {
    std::cout << "startStep " << startStep << " past the step count " << limit
              << ", clamped\n";
    startStep = limit;
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
  if (!exclusionMeshFile.empty()) {
    oss << "exclusionMesh " << ExclusionMeshPath() << "\n";
  }
  oss << "output " << OutputFolder() << "\n";
  oss << "dx " << dx << " containerSDFDx " << containerSDFDx
      << " broadPhaseDx " << broadPhaseDx << " gridDx " << gridDx
      << " subgridCellSize " << subgridCellSize << "\n";
  oss << "maxTrialCount " << maxTrialCount << " maxSecondsPerStep " << maxSecondsPerStep
      << " startStep " << startStep;
  if (endStep != std::numeric_limits<unsigned>::max()) {
    oss << " endStep " << endStep;
  }
  oss << " startItem " << startItem << "\n";
  oss << "resume " << resume;
  if (resume) {
    oss << " " << ResumePackPath();
  }
  oss << "\n";
  oss << "trajSaveInterval " << trajSaveInterval << " packSaveInterval "
      << packSaveInterval << " computeStats " << computeStats << "\n";
  return oss.str();
}
