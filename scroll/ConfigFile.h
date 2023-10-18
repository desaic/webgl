#pragma once
#include <map>
#include <string>

class ConfigFile {
 public:
  ConfigFile() {}
  std::map<std::string, std::string> val;
};