#pragma once

#include <string>
#include <vector>
struct PcdPoints {
  std::vector<float> _data;
  //3 floats for xyz, 4 for xyz then RGB
  unsigned floatsPerPoint = 3;
  void Allocate(unsigned numPoints) {
    _data.resize(numPoints * floatsPerPoint);
  }
  size_t NumPoints() const { return _data.size() / floatsPerPoint; }
  size_t NumBytes() const {return _data.size() * sizeof(float);}
  float* data() { return _data.data(); }
  const float* data() const{ return _data.data(); }

  float* operator[](size_t i) { return &(_data[i * floatsPerPoint]);}
  const float* operator[](size_t i) const{ return &(_data[i * floatsPerPoint]); }
};
void LoadPCD(const std::string& file, PcdPoints& points);