#pragma once
#include "DataPoint.h"
#include "ann.h"

class ANNTrain {
 public:
  // shift input x to zero mean
  // within each data point.
  // stores mean in _xCenter
  void CenterInput();
  //take one step along gradient
  //returns negative if object did not change much.
  int GradStep();
  std::shared_ptr<ANN> _ann;
  std::vector<DataPoint> _data;
  std::vector<float> _xCenter;
};
