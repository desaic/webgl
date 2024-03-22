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
  float LineSearch(const std::vector<Array2Df>& dw, float h0);
  /// <summary>
  /// to be minimized
  /// </summary>
  /// <returns></returns>
  float Objective();
  std::shared_ptr<ANN> _ann;
  std::vector<DataPoint> _data;
  std::vector<float> _xCenter;
};
