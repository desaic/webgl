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

float ANNTrain::Objective() {
  double sum = 0;
  // 1/2 Sum_i (y_i - yTruth_i)^2
  for (size_t i = 0; i < _data.size(); i++) {
    float y = 0;
    _ann->f(_data[i].x.data(), _data[i].x.size(), &y, 1);
    float yTruth = _data[i].y[0];
    float diff = y - yTruth;
    sum += 0.5f * diff * diff;
  }
  return float(sum);
}

float ANNTrain::LineSearch(const std::vector<Array2Df> & dw, float h0) {
  const int MAX_TRIALS = 20;
  float h = h0;
  float obj0 = Objective();
  std::vector<Array2Df> w0 = _ann->GetWeightsLayers();
  std::cout << "line search 0: " << obj0 << "\n";
  for (int i = 0; i < MAX_TRIALS; i++) {
    std::vector<Array2Df> w = w0;
    AddTimes(w, dw, h);
    _ann->SetWeightsLayers(w);
    float obj = Objective();
    std::cout << h << ": " << obj << " , ";
    if (obj < obj0) {
      return h;
    }
    h /= 2;
  }
  return 0.0f;
}

float MaxAbs(const Array2Df& a) {
  float m = 0;
  for (size_t i = 0; i < a.GetData().size(); i++) {
    m = std::max(m, std::abs(a.GetData()[i]));
  }
  return m;
}

float MaxAbs(const std::vector<Array2Df>& a) {
  float m = 0;
  for (size_t i = 0; i < a.size(); i++) {
    m = std::max(MaxAbs(a[i]), m);
  }
  return m;
}

int ANNTrain::GradStep() { 
  std::vector<Array2Df> dw;
  for (size_t i = 0; i < _data.size(); i++) {
    float y = 0;
    if (i == 88) {
      std::cout << "debug\n"; 
    }

    _ann->f(_data[i].x.data(), _data[i].x.size(), &y, 1);
    float yTruth = _data[i].y[0];
    float diff = y - yTruth;
    Array2Df dOut(1, 1);
    dOut(0, 0) = diff;
    std::vector<Array2Df> dwi;
    _ann->dfdw(dwi, dOut);
    if (i == 0) {
      dw = dwi;
    } else {
      AddTimes(dw, dwi, 1);
    }
  }
  std::vector<Array2Df> w = _ann->GetWeightsLayers();
  float maxw = MaxAbs(w);
  float maxdw = MaxAbs(dw);
  float h0 = 0.1f;
  if (maxdw > 0 && maxw>1e-6f) {
   // h0 = maxw / maxdw;
  }
  if (h0 > _prev_h * 16) {
    h0 = _prev_h * 16;
  }
  //negative gradient direction to minimize objective
  Scale(dw, -1);
  float h = LineSearch(dw, h0);
  if (h <= 0) {
    //could not decrease objective
    return -1;
  }
  _prev_h = h;
  AddTimes(w, dw, h);
  _ann->SetWeightsLayers(w);
  float o = Objective();
  std::cout << "obj: " << o << "\n";
  return 0; 
}
