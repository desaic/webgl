#include "ann.h"

#include "ArrayUtil.h"
#include <iostream>

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
    for (unsigned col = 0; col < _weights.Cols(); col++) {
      dw(col, row) = _cache[col] * doidy;
    }
  }
  return 0;
}

void DenseLinearLayer::UpdateCache(const float* input, unsigned inputSize,
                                   const float* output, unsigned outSize) {
  _inputSize = inputSize;
  //+1 for bias
  _cache.resize(_inputSize + 1);
  for (size_t col = 0; col < _inputSize; col++) {
    _cache[col] = input[col];
  }
  _cache[_inputSize] = 1;
}

void ActivationLayer::ApplyRelu(const float* input, unsigned inputSize,
                                float* output) {
  for (size_t i = 0; i < inputSize; i++) {
    output[i] = (input[i] < 0) ? 0 : input[i];
  }
}

void ActivationLayer::ApplyLRelu(const float* input, unsigned inputSize,
                                 float* output, float c) {
  for (size_t i = 0; i < inputSize; i++) {
    output[i] = (input[i] < 0) ? (c * input[i]) : input[i];
  }
}

int ActivationLayer::f_imp(const float* input, unsigned inputSize,
                           float* output, unsigned outSize) {
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
  float c = 0;
  dx.Allocate(_numInput, dody.Rows());
  if (_fun == LRelu) {
    c = param.size() > 0 ? param[0] : 0.01;
  }
  for (unsigned row = 0; row < dody.Rows(); row++) {
    for (size_t i = 0; i < _numInput; i++) {
      dx(i, row) = (_cache[i] < 0) ? c : 1;
      dx(i, row) *= dody(i, row);
    }
  }
  return 0;
}

int ActivationLayer::dodw(Array2Df& dw, const Array2Df& dody, unsigned oi) {
  // fixed function. no weights.
  return 0;
}

void ActivationLayer::UpdateCache(const float* input, unsigned inputSize,
                                  const float* output, unsigned outSize) {
  _inputSize = inputSize;
  _cache.resize(_inputSize);
  for (size_t col = 0; col < _inputSize; col++) {
    _cache[col] = input[col];
  }
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
  // for each filter
  unsigned weightCols = _weights.GetSize()[0];
  unsigned numFilters = _weights.GetSize()[1];

  for (unsigned f = 0; f < numFilters; f++) {
    const float* w = _weights.DataPtr() + f * weightCols;
    unsigned outCol = f;
    // compute convolution
    for (unsigned outRow = 0; outRow < outputRows; outRow++) {
      float sum = w[weightCols - 1];
      unsigned i0 = outRow * _stride;
      for (unsigned i = 0; i < _numInput; i++) {
        sum += w[i] * input[i0 + i];
      }
      output[outRow * outputCols + outCol] = sum;
    }
  }
  // input values are cached
  _inputSize = inputSize;
  _cache.resize(_inputSize);
  for (size_t i = 0; i < _inputSize; i++) {
    _cache[i] = input[i];
  }
  return 0;
}

// derivative of each output w.r.t input. num input cols x num output rows
int Conv1DLayer::dodx(Array2Df& dx, const Array2Df& dody) {
  if (_inputSize == 0 || _cache.size() != _inputSize) {
    return -1;
  }
  unsigned outputCols = _numNodes;
  unsigned outputRows = NumWindows(_inputSize);
  // for each filter
  unsigned numFilters = _weights.GetSize()[1];
  dx.Allocate(_inputSize, dody.Rows());
  dx.Fill(0);

  for (unsigned o = 0; o < dody.Rows(); o++) {
    for (unsigned f = 0; f < numFilters; f++) {
      const float* w = _weights.DataPtr() + f * _weights.Cols();
      // compute convolution
      for (unsigned outRow = 0; outRow < outputRows; outRow++) {
        unsigned i0 = outRow * _stride;
        unsigned yi = outRow * outputCols + f;
        for (unsigned i = 0; i < _numInput; i++) {
          unsigned xi = i0 + i;
          dx(xi, o) += _weights(i, f) * dody(yi, o);
        }
      }
    }
  }
  return 0;
}

// derivative w.r.t weight. num weights cols x num output rows.
int Conv1DLayer::dodw(Array2Df& dw, const Array2Df& dody, unsigned oi) {
  if (_inputSize == 0 || _cache.size() != _inputSize) {
    return -1;
  }
  unsigned outputCols = _numNodes;
  unsigned outputRows = NumWindows(_inputSize);
  // for each filter
  unsigned numFilters = _weights.GetSize()[1];
  dw.Allocate(_weights.GetSize());
  dw.Fill(0);

  for (unsigned f = 0; f < numFilters; f++) {
    const float* w = _weights.DataPtr() + f * _weights.Cols();
    // compute convolution
    for (unsigned outRow = 0; outRow < outputRows; outRow++) {
      unsigned i0 = outRow * _stride;
      unsigned yi = outRow * outputCols + f;
      for (unsigned i = 0; i < _numInput; i++) {
        unsigned xi = i0 + i;
        dw(i, f) += _cache[xi] * dody(yi, oi);
      }
      // bias gradient
      dw(_weights.Cols() - 1, f) += dody(yi, oi);
    }
  }

  return 0;
}

void Conv1DLayer::UpdateCache(const float* input, unsigned inputSize,
                              const float* output, unsigned outSize) {
  _inputSize = inputSize;
  _cache.resize(_inputSize);
  for (size_t i = 0; i < _inputSize; i++) {
    _cache[i] = input[i];
  }
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

int ANN::dfdw(std::vector<Array2Df>& dwLayer, const Array2Df& dOut) {
  if (_layers.empty()) {
    return 0;
  }
  auto lastLayer = _layers.back();
  unsigned lastNumOutput = lastLayer->NumOutput(lastLayer->InputSizeCached());
  Array2Df dody = dOut;
  dwLayer.resize(_layers.size());
  for (int i = int(_layers.size()) - 1; i >= 0; i--) {
    std::shared_ptr<Layer> l = _layers[size_t(i)];
    unsigned outSize = l->NumOutput(l->InputSizeCached());
    l->dodw(dwLayer[i], dody, 0);
    Array2Df dx;
    l->dodx(dx, dody);
    dody = dx;
  }
  return 0;
}

std::vector<float> ANN::GetWeights() const {
  std::vector<float> weights;
  for (size_t i = 0; i < _layers.size(); i++) {
    const Layer& l = *_layers[i];
    weights.insert(weights.end(), l._weights.GetData().begin(),
                   l._weights.GetData().end());
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

std::vector<Array2Df> ANN::GetWeightsLayers() const {
  std::vector<Array2Df> w(_layers.size());
  for (size_t i = 0; i < w.size(); i++) {
    w[i] = _layers[i]->_weights;
  }
  return w;
}

void ANN::SetWeightsLayers(const std::vector<Array2Df>& w) {
  if (w.size() != _layers.size()) {
    return;
  }
  for (size_t i = 0; i < w.size(); i++) {
    _layers[i]->_weights = w[i];
  }
}

void ANN::SaveWeights(std::ostream& out) const {
  for (size_t i = 0; i < _layers.size(); i++) {
    const Array2Df& w = _layers[i]->_weights;
    out << w.Cols() << " " << w.Rows() << "\n";
    for (unsigned row = 0; row < w.Rows(); row++) {
      for (unsigned col = 0; col < w.Cols(); col++) {
        out << w(col, row) << " ";
      }
      out << "\n";
    }
    out<<"\n";
  }
}