#pragma once
#include <map>
#include <string>
#include <iostream>

class ConfigFile {
 public:
  ConfigFile() {}
  void Load(std::istream& in);
  void Save(std::ostream& out);
  std::string dataDir = "H:/segments";
};