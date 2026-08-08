#include "PackingPlan.h"

#include "Log.h"
#include "MeshInfo.h"
#include "TrigMesh.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>

namespace fs = std::filesystem;

static const char *FillModeName(FillMode m) {
  return m == FillMode::FillSurface ? "fillSurface" : "fillVolume";
}

std::string PackingStep::toString() const {
  std::ostringstream oss;
  oss << "names " << names.size() << " ";
  for (size_t i = 0; i < names.size(); i++) {
    oss << names[i] << " ";
  }
  oss << " force " << force[0] << " " << force[1] << " " << force[2] << " ";
  oss << "count " << count;
  oss << " outwards " << outwards << " useInnerContainer " << useInnerContainer;
  oss << " fillMode " << FillModeName(fillMode);
  return oss.str();
}

void PackingStep::Load(std::istream &in) {
  std::string token;
  in >> token; // "names"
  size_t numNames;
  in >> numNames;
  names.resize(numNames);
  for (size_t i = 0; i < numNames; i++) {
    in >> names[i];
  }
  in >> token; // "force"
  in >> force[0] >> force[1] >> force[2];
  in >> token; // "count"
  in >> count;
  in >> token; // "outwards"
  in >> outwards;
  in >> token; // "useInnerContainer"
  in >> useInnerContainer;

  // optional trailing token: absent in plans saved before H5, and also
  // absent here at the very last step of the file, where the next read
  // hits eof rather than the next step's "names" token.
  fillMode = FillMode::FillVolume;
  std::streampos pos = in.tellg();
  std::string maybeKey;
  if (in >> maybeKey) {
    if (maybeKey == "fillMode") {
      std::string modeStr;
      in >> modeStr;
      fillMode = (modeStr == "fillSurface") ? FillMode::FillSurface : FillMode::FillVolume;
    } else {
      in.seekg(pos);
    }
  } else {
    in.clear();
    in.seekg(pos);
  }
}

void PackingPlan::Save(std::ostream &out) const {
  out << "groups " << groups.size() << "\n";
  for (size_t i = 0; i < groups.size(); i++) {
    out << groups[i].size() << " ";
    for (size_t j = 0; j < groups[i].size(); j++) {
      out << groups[i][j] << " ";
    }
    out << "\n";
  }
  out << "num_steps " << steps.size() << "\n";
  for (size_t i = 0; i < steps.size(); i++) {
    out << steps[i].toString() << "\n";
  }
}

void PackingPlan::Load(std::istream & in){
  std::string token;
  in >> token; // "groups"
  size_t numGroups;
  in >> numGroups;
  groups.resize(numGroups);
  for (size_t i = 0; i < numGroups; i++) {
    size_t groupSize;
    in >> groupSize;
    groups[i].resize(groupSize);
    for (size_t j = 0; j < groupSize; j++) {
      in >> groups[i][j];
    }
  }
  in >> token; // "num_steps"
  size_t numSteps;
  in >> numSteps;
  steps.resize(numSteps);
  for (size_t i = 0; i < numSteps; i++) {
    steps[i].Load(in);
  }
}

float MeshStat::MaxExtent() const {
  Vec3f boxSize = box.vmax - box.vmin;
  return std::max(std::max(boxSize[0], boxSize[1]), boxSize[2]);
}

std::string MeshStat::toString() const {
  std::ostringstream oss;
  oss << name;
  oss << " box " << box.vmin[0] << " " << box.vmin[1] << " " << box.vmin[2]
      << " " << box.vmax[0] << " " << box.vmax[1] << " " << box.vmax[2];
  Vec3f boxSize = box.vmax - box.vmin;
  oss << " size " << boxSize[0] << " " << boxSize[1] << " " << boxSize[2];
  return oss.str();
}

void MeshStat::Parse(std::istream &in) {
  in >> name;
  std::string token;
  in >> token;
  in >> box.vmin[0] >> box.vmin[1] >> box.vmin[2] >> box.vmax[0] >> box.vmax[1]
      >> box.vmax[2];
  // size can be derived from box, so it's not stored in struct.
  // it's stored in file for human readability
  in >> token;
  Vec3f size;
  in >> size[0] >> size[1] >> size[2];
}

static bool IsMeshExt(const fs::path &p) {
  std::string ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return ext == ".obj" || ext == ".stl";
}

void ComputeMeshStats(const std::string &meshDir) {
  fs::path inPath(meshDir);
  std::string statsFile = meshDir + "/stats.txt";

  if (!fs::exists(inPath) || !fs::is_directory(inPath)) {
    std::cout << inPath.string() << " not a valid directory\n";
    return;
  }
  std::ofstream out(statsFile);
  for (const auto &entry : fs::directory_iterator(inPath)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    fs::path p = entry.path();
    if (!IsMeshExt(p)) {
      LOGD("unknown extension " << p.extension().string() << " skip\n");
      continue;
    }
    LOGD("stats " << p.filename().string() << "\n");

    TrigMesh mesh;
    if (LoadMesh(mesh, p) != 0) {
      std::cout << "error loading " << p.filename().string() << "\n";
      continue;
    }
    MeshStat stat;
    stat.name = p.stem().string();
    stat.box = ComputeBBox(mesh.v);
    out << stat.toString() << "\n";
  }
  out.close();
}

std::vector<MeshStat> LoadMeshStats(const std::string &meshDir) {
  std::vector<MeshStat> stats;
  std::ifstream statsFile(meshDir + "/stats.txt");
  const unsigned MIN_STAT_LEN = 3;
  std::string line;
  while (std::getline(statsFile, line)) {
    if (line.size() < MIN_STAT_LEN) {
      continue;
    }
    MeshStat stat;
    std::istringstream iss(line);
    stat.Parse(iss);
    stats.push_back(stat);
  }
  return stats;
}

unsigned GetGroupIndex(float len, const std::vector<float> &thresh) {
  for (unsigned i = 0; i < thresh.size(); i++) {
    if (len > thresh[i]) {
      return i;
    }
  }
  return unsigned(thresh.size());
}

PackingPlan PlanPackingSteps(const std::string &meshDir) {
  std::vector<MeshStat> stats = LoadMeshStats(meshDir);
  LOGI("loaded " << stats.size() << " stats\n");
  if (stats.empty()) {
    return PackingPlan();
  }

  // >20 large, medium large, medium small, small
  std::vector<float> SIZE_THRESH = {20, 6, 3};

  PackingPlan plan;
  plan.groups.resize(SIZE_THRESH.size() + 1);

  // one pass: assign to a group and remember the extent for sorting.
  std::map<std::string, float> nameToLen;
  for (size_t i = 0; i < stats.size(); i++) {
    float len = stats[i].MaxExtent();
    nameToLen[stats[i].name] = len;
    unsigned gid = GetGroupIndex(len, SIZE_THRESH);
    plan.groups[gid].push_back(stats[i].name);
  }

  // largest first within each group.
  for (size_t gid = 0; gid < plan.groups.size(); gid++) {
    auto &group = plan.groups[gid];
    std::sort(group.begin(), group.end(),
              [&nameToLen](const std::string &a, const std::string &b) {
                return nameToLen[a] > nameToLen[b];
              });
  }

  // put 20 medium fruits towards the left
  PackingStep step0;
  step0.names = plan.groups[1];
  step0.count = 20;
  step0.force = Vec3f(-10, 0, 0);
  plan.steps.push_back(step0);

  // pack as many big then medium fruits as possible towards outside of container.
  const unsigned LARGE_INT = 1000000u;
  for (unsigned g = 0; g < plan.groups.size() - 1; g++) {
    PackingStep step;
    step.names = plan.groups[g];
    step.count = LARGE_INT;
    plan.steps.push_back(step);
  }

  Vec3f finalForce(-0.1f, 0, 0);
  // pack small fruits into surface pockets (heuristic_plan.md H2 onward).
  // useInnerContainer was true here for the old lattice-walk fill, which
  // needed it to stop wasting trials deep in the middle of the container.
  // Pocket targeting never searches there in the first place (it only
  // aims at rays cast a bounded depth in from the real surface), and
  // measured with it on anyway: invalid rays 78 -> 893 and several open
  // ray depth buckets collapsed to a fraction of their size, because the
  // inner shell reads as a hit in the very class grid pocket targeting
  // marches through -- it hides real pockets behind a wall that will not
  // exist in the printed/milled part. Visible berry fraction was a wash
  // (0.773 vs 0.766 in a 20s A/B on the same resumed pack), so there is no
  // upside to offset that. Left off.
  PackingStep lastStep;
  lastStep.names = plan.groups.back();
  lastStep.outwards = false;
  lastStep.useInnerContainer = false;
  lastStep.count = LARGE_INT;
  lastStep.force = finalForce;
  lastStep.fillMode = FillMode::FillSurface;
  plan.steps.push_back(lastStep);


  return plan;
}