#include "Vec2.h"
#include "Vec3.h"
#include <string>
#include <sstream>

template <typename T> 
std::string to_string(const Vec3<T> & v){
  std::stringstream ss;
  ss<<"["<< v[0]<<", "<<v[1]<<", "<<v[2]<<"]";
  return ss.str();
}

template <typename T> 
std::string to_string(const Vec2<T> & v){
  std::stringstream ss;
  ss<<"["<< v[0]<<", "<<v[1]<<"]";
  return ss.str();
}
