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

void ReadString(const std::string& key, std::string& val,
                const std::map<std::string, std::string>&conf) {
  auto it = conf.find(key);
  if (it == conf.end()) {
    return;
  }
  val = it->second;
}

void ConfigFile::Load(std::istream& in) {
  std::string line;
  std::map<std::string, std::string> confMap;
  while (std::getline(in, line)) {
    std::istringstream iss(line);
    std::string key, val;
    std::getline(iss, key, '=');
    std::getline(iss, val, '=');
    key = trim(key);
    val = trim(val);
    confMap[key] = val;
  }
  ReadString("dataDir", dataDir, confMap);
}

void ConfigFile::Save(std::ostream& out) {
  out << "dataDir" << "=" << dataDir << std::endl;

}