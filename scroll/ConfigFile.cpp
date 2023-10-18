#include "ConfigFile.h"
#include <sstream>
void ConfigFile::Load(std::istream& in) {
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string key, val;
    std::getline(iss, key, '=');
    std::getline(iss, val, '=');
    conf[key] = val;
  }
}

void ConfigFile::Save(std::ostream& out) {
}