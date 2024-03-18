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

int ANNTrain::GradStep() { 
  std::vector<float> w = _ann->GetWeights();
  std::vector<float> dw(w.size(), 0);
  for (size_t i = 0; i < _data.size(); i++) {
    float y = 0;
    _ann->f(_data[i].x.data(), _data[i].x.size(), &y, 1);
    std::vector<float> dwi;
    _ann->dfdw(_data[i].x.data(), _data[i].x.size(), dw.data(), dw.size());
    Add(dw, dwi);
  }
  return 0; 
}
