#include "StringUtil.h"
std::string get_suffix(const std::string& str,
                       const std::string& delimiters) {
  size_t pos = str.find_last_of(delimiters);
  if (pos == std::string::npos) {
    return "";  // No suffix found
  }
  return str.substr(pos + 1);
}

std::string remove_suffix(const std::string& str, const std::string& suffix) {
  size_t pos = str.rfind(suffix);
  if (pos != std::string::npos) {
    return str.substr(0, pos);
  } else {
    return str;  // Suffix not found, return original string
  }
}