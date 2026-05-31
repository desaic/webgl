#include "PackingPlan.h"

std::string PackingStep::toString() const {
  std::ostringstream oss;
  oss << "names " << names.size() << " ";
  for (size_t i = 0; i < names.size(); i++) {
    oss << names[i] << " ";
  }
  oss << " force " << force[0] << " " << force[1] << " " << force[2] << " ";
  oss << "count " << count;
  oss << " outwards " << outwards << " useInnerContainer " << useInnerContainer;
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