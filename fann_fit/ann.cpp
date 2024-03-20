#include "ann.h"
#include "ArrayUtil.h"
int DenseLinearLayer::f_imp(const float* input, unsigned inputSize,
                            float* output, unsigned outSize) {
  if (inputSize != _numInput || outSize != _weights.Rows()) {
    return -1;
  }
  MVProd(_weights, input, inputSize, output, outSize);
  // add bias
  for (unsigned row = 0; row < _weights.Rows(); row++) {
    output[row] += _weights(_weights.Cols() - 1, row);
  }
  return 0;
}

// derivative w.r.t input. num input cols x num output rows
// y = Wx + b. dy/dx = W. do/dx = do/dy * dy/dx 
int DenseLinearLayer::dodx(Array2Df& dx, const Array2Df& dody) {
  //TODO: implement
  unsigned oSize = dody.Rows();
  unsigned inSize = _inputSize;
  dx.Allocate(inSize, oSize);
  if (_weights.Rows() < dody.Cols()) {
    return -1;
  }
  for (unsigned row = 0; row < oSize; row++) {
    const float* srcRow = dody.DataPtr() + row * dody.Cols();
    float* dstRow = dx.DataPtr() + row * dx.Cols();
    MTVProd(_weights, srcRow, dody.Cols(), dstRow, dx.Cols());
  }
  return 0;
}

// derivative w.r.t weight. num weights cols x num output rows.
// do/dw = do/dy * dy/dw. dyi/dwij = xj
int DenseLinearLayer::dodw(Array2Df& dw, const Array2Df& dody, unsigned oi) {
  dw.Allocate(_weights.GetSize());
  for (unsigned row = 0; row < _weights.Rows(); row++) {
    float doidy = dody(row, oi);
    for (unsigned col = 0; col < _weights.Cols() - 1; col++) {
      dw(col, row) = _cache[col] * doidy;
    }
    //bias term
    dw(_weights.Cols() - 1, row) = doidy;
  }
  return 0;
}

void DenseLinearLayer::UpdateCache(const float* input, unsigned inputSize,
                                   const float* output, unsigned outSize) {
  _inputSize = inputSize;
  _cache.resize(_inputSize);
  for (size_t col = 0; col < _inputSize; col++) {
    _cache[col] = input[col];
  }
}

void ActivationLayer::ApplyRelu(const float* input, unsigned inputSize,
                                float* output) {
  for (size_t i = 0; i < inputSize; i++) {
    output[i] = (input[i] < 0) ? 0 : input[i];
  }
}

void ActivationLayer::ApplyLRelu(const float* input, unsigned inputSize,
                                float* output,float c) {
  for (size_t i = 0; i < inputSize; i++) {
    output[i] = (input[i] < 0) ? (c * input[i]) : input[i];
  }
}

int ActivationLayer::f_imp(const float* input, unsigned inputSize,
                           float* output,
                       unsigned outSize) {
  float c = param.size() > 0 ? param[0] : 0.01;
  if (inputSize != _numInput || outSize != _numNodes || inputSize != outSize) {
    return -1;
  }

  switch (_fun) {
    case Relu:
      ApplyRelu(input, inputSize, output);
      break;
    case LRelu:
      ApplyLRelu(input, inputSize, output, c);
      break;
    default:
      break;
  }
  
  _cache.resize(inputSize);
  for (size_t i = 0; i < inputSize; i++) {
    _cache[i] = input[i];
  }
  return 0;
}

int ActivationLayer::dodx(Array2Df& dx, const Array2Df& dody) {
  float c = param.size() > 0 ? param[0] : 0.01;
  switch (_fun) {
    case Relu:
      for (size_t i = 0; i < _numInput; i++) {
        dx[i] = (_cache < 0) ? 0 : 1;
      }
      break;
    case LRelu:
      for (size_t i = 0; i < _numInput; i++) {
        dx[i] = (_cache[i] < 0) ? c : 1;
      }
      break;
    default:
      break;
  }
  return 0;
}

int ActivationLayer::dfdw(float* dw, const Array2Df& dody) {
  //fixed function. no weights.
  return 0;
}

int Conv1DLayer::f_imp(const float* input, unsigned inputSize, float* output,
                   unsigned outSize) {
  unsigned outputCols = _numNodes;
  unsigned outputRows = NumWindows(inputSize);
  if (outSize != outputCols * outputRows) {
    return -1;
  }
  if (_pad > 0) {
  
  }
  //for each filter
  unsigned weightCols = _weights.GetSize()[0];
  unsigned numFilters = _weights.GetSize()[1];

  for (unsigned f = 0; f < numFilters; f++) {
    const float* w = _weights.DataPtr() + f * weightCols;
    unsigned outCol = f;
    //compute convolution
    for (unsigned outRow = 0; outRow < outputRows; outRow++) {
      float sum = w[weightCols-1];
      unsigned i0 = outRow * _stride;
      for (unsigned i = 0; i < _numInput; i++) {
        sum += w[i] * input[i0 + i];
      }
      output[outRow * outputCols + outCol] = sum;
    }
  }
  //input values are cached
  _inputSize = inputSize;
  _cache.resize(_inputSize);
  for (size_t i = 0; i < _inputSize; i++) {
    _cache[i] = input[i];
  }
  return 0;
}

// derivative of each output w.r.t input. num input cols x num output rows
int Conv1DLayer::dfdx(float* dx, const Array2Df& dody) {
  return 0;
}

// derivative w.r.t weight. num weights cols x num output rows.
int Conv1DLayer::dfdw(float* dw, const Array2Df& dody) {
  return 0;
}

int ANN::f(const float* input, unsigned inputSize, float* output,
           unsigned outSize) {
  std::vector<float> layerIn(inputSize), layerOut;
  for (size_t i = 0; i < inputSize; i++) {
    layerIn[i] = input[i];
  }
  for (size_t i = 0; i < _layers.size(); i++) {
    Layer& l = *_layers[i];
    unsigned outSize = l.NumOutput(layerIn.size());    
    layerOut.resize(outSize);
    _layers[i]->F(layerIn.data(), layerIn.size(), layerOut.data(), outSize);
    layerIn = layerOut;
  }
  if (layerOut.size() != outSize) {
    return -1;
  }
  
  for (size_t i = 0; i < outSize; i++) {
    output[i] = layerOut[i];
  }
  return 0;
}

int ANN::dfdw(const float* input, unsigned inputSize, float* dw,
         unsigned wSize) {
  if (_layers.empty()) {
    return 0;
  }
  auto lastLayer = _layers.back();
  unsigned lastNumOutput = lastLayer->NumOutput(lastLayer->InputSizeCached());
  Array2Df dodyEnd(lastNumOutput, 1);
  dodyEnd.Fill(1);
  Array2Df dody = dodyEnd;
  for (int i = int(_layers.size()) - 1; i >= 0; i--) {
    std::shared_ptr<Layer> l = _layers[size_t(i)];
    unsigned outSize = l->NumOutput(l->InputSizeCached());
    l->dfdw(dw,dody);
  }
  return 0;
}

std::vector<float> ANN::GetWeights() const {
  std::vector<float> weights;
  for (size_t i = 0; i < _layers.size(); i++) {
    const Layer& l = *_layers[i];
    weights.insert(weights.end(), l._weights.GetData().begin(), l._weights.GetData().end());
  }
  return weights;
}

void ANN::SetWeights(const std::vector<float>& weights) {
  size_t wIdx = 0;
  for (size_t i = 0; i < _layers.size(); i++) {
    Layer& l = *_layers[i];
    for (size_t j = 0; j < l._weights.GetData().size(); j++) {
      l._weights.GetData()[j] = weights[wIdx];
      wIdx++;
    }
  }
}
