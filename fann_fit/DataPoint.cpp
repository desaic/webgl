#include "DataPoint.h"
#include <sstream>

std::string DataPoint::ToString() const {
  std::ostringstream oss;
  //oss << name << " ";
  oss << x.size() << " ";
  for (size_t i = 0; i < x.size(); i++) {
    oss << x[i] << " ";
  }
  oss << y.size() << " ";
  for (size_t i = 0; i < y.size(); i++) {
    oss << y[i] << " ";
  }
  return oss.str();
}

void DataPoint::Read1D(std::istream& in) {
  std::string name;
  in >> name;
  int xSize = 0;
  in >> xSize;
  x.resize(xSize, 0);
  for (size_t i = 0; i < size_t(xSize); i++) {
    in >> x[i];
  }
  y.resize(1,0);
  in >> y[0];
}
