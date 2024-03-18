#include "ann_train.h"
#include "ArrayUtil.h"

void ANNTrain::CenterInput() {
  _xCenter.resize(_data.size(), 0);
  for (size_t i = 0; i < _data.size(); i++) {
    if (_data[i].x.size() == 0) {
      continue;
    }
    float sum = Sum(_data[i].x);
    sum /= _data[i].x.size();
    AddConstant(_data[i].x, -sum);
    _xCenter[i] = sum;
  }
}