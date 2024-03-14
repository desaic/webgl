#ifndef ANN_H
#define ANN_H

#include <memory>
#include <vector>
#include <string>
#include "../Math/Array2D.h"
class Layer {
 public:
  Layer(unsigned numInput, unsigned numNodes)
      : _numInput(numInput), _numNodes(numNodes) {
  }
  virtual int f(const float* input, unsigned inputSize, float* output,
                unsigned outSize) = 0;
  // derivative w.r.t input. num input cols x num output rows
  virtual int dfdx(float* df, unsigned inputSize, unsigned outSize) = 0;
  // derivative w.r.t weight. num weights cols x num output rows.
  virtual int dfdw(float* df, unsigned weightSize, unsigned outSize) = 0;

  virtual unsigned NumOutput() const { return _numNodes; }
  virtual unsigned NumWeights() const { return _weights.size(); }

  unsigned _numInput = 1;
  unsigned _numNodes = 1;
  std::vector<float> _cache;
  Array2Df _weights;
};

// weights are stored in numOutput Rows x numInput cols.
// input size is fixed unlike cnn layer. _cache is used to
// store input values.
class DenseLinearLayer : public Layer {
 public:
  DenseLinearLayer(unsigned numInput, unsigned numOutput)
      : Layer(numInput, numOutput) {
    //+1 for bias weight.
    _weights.resize( (numInput + 1) * numOutput);
  }
  virtual int f(const float* input, unsigned inputSize, float* output,
                unsigned outSize);
  // derivative w.r.t input. num input cols x num output rows
  virtual int dfdx(float* df, unsigned inputSize, unsigned outSize);
  // derivative w.r.t weight. num weights cols x num output rows.
  virtual int dfdw(float* df, unsigned weightSize, unsigned outSize);

  unsigned NumWeights() const override { return 0; }
};

class ActivationLayer : public Layer {
 public:
  enum ActFun { Relu, LRelu };
  //no weights. fixed function.
  ActivationLayer(unsigned numInput, enum ActFun type)
      : Layer(numInput, numInput), _fun(type) {}
  ActFun _fun = Relu;

  void ApplyRelu(const float* input, unsigned inputSize, float* output);
  void ApplyLRelu(const float* input, unsigned inputSize, float* output, float c);
  virtual int f(const float* input, unsigned inputSize, float* output,
                unsigned outSize) override;
  // derivative w.r.t input. num input cols x num output rows
  virtual int dfdx(float* df, unsigned inputSize, unsigned outSize) override;
  // derivative w.r.t weight. num weights cols x num output rows.
  virtual int dfdw(float* df, unsigned weightSize, unsigned outSize) override;

  std::vector<float> param;
};

class Conv1DLayer : public Layer {
 public:
  Conv1DLayer(unsigned window, unsigned stride, unsigned pad,
              unsigned numOutput)
      : Layer(window, numOutput), _stride(stride), _pad(pad) {
    //+1 for weight for bias input.
    _weights.resize( (window + 1) * numOutput);
  }
  virtual int f(const float* input, unsigned inputSize, float* output,
                unsigned outSize) override;
  // derivative w.r.t input. num input cols x num output rows
  virtual int dfdx(float* df, unsigned inputSize, unsigned outSize) override;
  // derivative w.r.t weight. num weights cols x num output rows.
  virtual int dfdw(float* df, unsigned weightSize, unsigned outSize) override;

  unsigned NumOutput() const override { return _numNodes * NumWindows(); }

  unsigned NumWindows() const {
    unsigned window = _numInput;
    if (_inputSize + 2 * _pad < window) {
      return 1;
    }
    return (_inputSize + 2 * _pad - window) / _stride + 1;
  }

  unsigned _pad = 0;
  unsigned _stride = 1;
  // state that updates depending on input
  unsigned _inputSize = 1;
};

class ANN {
 public:
  int f(const float* input, unsigned inputSize, float* output,
        unsigned outSize);

  int dfdw(const float* input, unsigned inputSize, float* output,
           unsigned outSize);

  void AddLayer(std::shared_ptr<Layer> layer) { _layers.push_back(layer); }
  //weights of all layers ordered from input to output.
  std::vector<float> GetWeights() const;
  void SetWeights(const std::vector<float>& weights);

  std::vector<std::shared_ptr<Layer> > _layers;
};

#endif