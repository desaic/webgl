#include "ConfigFile.h"
#include <sstream>

std::string trim(const std::string& str) {
  size_t first = str.find_first_not_of(' ');
  size_t last = str.find_last_not_of(' ');

  if (first == std::string::npos) {
    return "";  // If the string is all spaces
  }

  return str.substr(first, (last - first + 1));
}

void ConfigFile::Load(std::istream& in) {
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string key, val;
    std::getline(iss, key, '=');
    std::getline(iss, val, '=');
    key = trim(key);
    val = trim(val);
    conf[key] = val;
  }
}

void ConfigFile::Save(std::ostream& out) {
  for (const auto& pair : conf) {
    out << pair.first << "=" << pair.second << std::endl;
  }
}