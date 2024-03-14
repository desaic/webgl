#include "ann.h"

int DenseLinearLayer::f(const float* input, unsigned inputSize, float* output,
                       unsigned outSize) {
  if (inputSize != _numInput) {
    return -1;
  }

  _cache.resize(_numInput);
  for (size_t col = 0; col < inputSize; col++) {
    _cache[col] = input[col];
  }
  unsigned weightCols = _numInput + 1;
  for (size_t row = 0; row < _numNodes; row++) {
    output[row] = _weights[(row + 1) * weightCols - 1];
    const float* w = (const float*)_weights.data() + row * weightCols;
    for (size_t col = 0; col < inputSize; col++) {
      output[row] += input[col] * w[col];
    }
  }
  return 0;
}

// derivative w.r.t input. num input cols x num output rows
int DenseLinearLayer::dfdx(float* df, unsigned inputSize, unsigned outSize) {
  if (inputSize != _numInput || outSize != _numNodes) {
    return -1;
  }
  for (size_t row = 0; row < _numNodes; row++) {
    for (size_t col = 0; col < _numInput; col++) {
      df[col + row * _numInput] = _weights[col + row * _numInput];
    }
  }
  return 0;
}

// derivative w.r.t weight. num weights cols x num output rows.
int DenseLinearLayer::dfdw(float* df, unsigned weightSize, unsigned outSize) {
  size_t numWeights = NumWeights();
  if (numWeights != weightSize) {
    return -1;
  }
  for (size_t row = 0; row < outSize; row++) {
    for (size_t col = 0; col < numWeights - 1; col++) {
      df[col + row * numWeights] = _cache[col];
    }
    // derivative for bias
    df[numWeights - 1 + row * numWeights] = 1;
  }
  return 0;
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

int ActivationLayer::f(const float* input, unsigned inputSize, float* output,
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

int ActivationLayer::dfdx(float* df, unsigned inputSize, unsigned outSize) {
  float c = param.size() > 0 ? param[0] : 0.01;
  switch (_fun) {
    case Relu:
      for (size_t i = 0; i < _numInput; i++) {
        df[i] = (_cache[i] < 0) ? 0 : 1;
      }
      break;
    case LRelu:
      for (size_t i = 0; i < _numInput; i++) {
        df[i] = (_cache[i] < 0) ? c : 1;
      }
      break;
    default:
      break;
  }
  return 0;
}

int ActivationLayer::dfdw(float* df, unsigned weightSize, unsigned outSize) {
  //fixed function. no weights.
  return 0;
}

int Conv1DLayer::f(const float* input, unsigned inputSize, float* output,
                   unsigned outSize) {
  unsigned rows = _numNodes;
  unsigned cols = NumWindows();
  if (outSize != rows * cols) {
    return -1;
  }
  if (_pad > 0) {
  
  }
  //for each filter
  for (unsigned row = 0; row < rows; row++) {
    const float* w = _weights.data() + row * _numInput;
    //compute convolution
    for (unsigned col = 0; col < cols; col++) {
      float sum = _bias[row];
      unsigned i0 = col * _stride;
      for (unsigned i = 0; i < _numInput; i++) {
        sum += w[i] * input[i0 + i];
      }
      output[row * cols + col] = sum;
    }
  }
  //input values are cached
  _inputSize = inputSize;
  _cache.resize(_inputSize);

  return 0;
}

// derivative of each output w.r.t input. num input cols x num output rows
int Conv1DLayer::dfdx(float* df, unsigned inputSize, unsigned outSize) {
  return 0;
}

// derivative w.r.t weight. num weights cols x num output rows.
int Conv1DLayer::dfdw(float* df, unsigned weightSize, unsigned outSize) {
  return 0;
}


int ANN::f(const float* input, unsigned inputSize, float* output,
           unsigned outSize) {
  return 0;
}

int ANN::dfdw(const float* input, unsigned inputSize, float* output,
         unsigned outSize) {
  return 0;
}

std::vector<float> ANN::GetWeights() const {
  std::vector<float> weights;
  for (size_t i = 0; i < _layers.size(); i++) {
    const Layer& l = *_layers[i];
    weights.insert(weights.end(), l._weights.begin(), l._weights.end());
  }
  return weights;
}

void ANN::SetWeights(const std::vector<float>& weights) {
  size_t wIdx = 0;
  for (size_t i = 0; i < _layers.size(); i++) {
    Layer& l = *_layers[i];
    for (size_t j = 0; j < l._weights.size(); j++) {
      l._weights[j] = weights[wIdx];
      wIdx++;
    }
  }
}
