#pragma once
#include <string>
#include <iostream>
#include <vector>

struct DataPoint {
  std::vector<float> x;
  std::vector<float> y;
  std::string ToString() const ;
  //read data with 1 output value.
  void Read1D(std::istream& in);
};
