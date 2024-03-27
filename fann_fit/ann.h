#ifndef ANN_H
#define ANN_H

#include <memory>
#include <vector>
#include <string>
#include "Array2D.h"

class Layer {
 public:
  Layer(unsigned numInput, unsigned numNodes)
      : _numInput(numInput), _numNodes(numNodes) {
  }
  int F(const float* input, unsigned inputSize, float* output,
        unsigned outSize) {
    int ret = f_imp(input, inputSize, output, outSize);
    UpdateCache(input, inputSize, output, outSize);
    return ret;
  }

  virtual int f_imp(const float* input, unsigned inputSize, float* output,
        unsigned outSize) = 0;
  /// derivative w.r.t input. num input cols x num output rows
  /// uses cached _inputSize and values
  /// @param dx output. row vector of do/dx
  /// @param dody do/dy output of the whole network w.r.t each output.
  virtual int dodx(Array2Df& dx, const Array2Df& dody) = 0;
  /// derivative of the ith network output w.r.t weight.
  /// num weights cols x num output rows.
  /// uses cached _inputSize and values  
  virtual int dodw(Array2Df& dw, const Array2Df& dody, unsigned oi) = 0;

  virtual unsigned NumOutput(unsigned inputSize) const { return _numNodes; }
  virtual unsigned NumWeights() const { return _weights.GetData().size(); }

  virtual unsigned InputSizeCached() const { return _inputSize; }

  /// update cached states.
  virtual void UpdateCache(const float* input, unsigned inputSize,
                           const float* output, unsigned outSize) = 0;

  unsigned _numInput = 1;
  unsigned _numNodes = 1;
  std::vector<float> _cache;
  Array2Df _weights;
  //state that updates depending on calls to f() 
  unsigned _inputSize = 0;
};

// weights are stored in numOutput Rows x numInput cols.
// input size is fixed unlike cnn layer. _cache is used to
// store input values.
class DenseLinearLayer : public Layer {
 public:
  DenseLinearLayer(unsigned numInput, unsigned numOutput)
      : Layer(numInput, numOutput) {
    //+1 for bias weight.
    _weights.Allocate( numInput + 1 , numOutput);
  }
  virtual int f_imp(const float* input, unsigned inputSize, float* output,
                unsigned outSize);
  // derivative w.r.t input. num input cols x num output rows
  virtual int dodx(Array2Df& dx, const Array2Df& dody);
  // derivative w.r.t weight. num weights cols x num output rows.
  virtual int dodw(Array2Df& dx, const Array2Df& dody, unsigned oi);

  void UpdateCache(const float* input, unsigned inputSize, const float* output,
                   unsigned outSize) override;
};

class ActivationLayer : public Layer {
 public:
  enum ActFun { Relu, LRelu, Tanh };
  //no weights. fixed function.
  ActivationLayer(unsigned numInput, enum ActFun type)
      : Layer(numInput, numInput), _fun(type) {}
  ActFun _fun = Relu;

  void ApplyRelu(const float* input, unsigned inputSize, float* output);
  void ApplyLRelu(const float* input, unsigned inputSize, float* output, float c);
  void ApplyTanh(const float* input, unsigned inputSize, float* output);
  virtual int f_imp(const float* input, unsigned inputSize, float* output,
                unsigned outSize) override;
  // derivative w.r.t input. num input cols x num output rows
  virtual int dodx(Array2Df& dx, const Array2Df& dody) override;
  // derivative w.r.t weight. num weights cols x num output rows.
  virtual int dodw(Array2Df& dw, const Array2Df& dody, unsigned oi) override;
  void UpdateCache(const float* input, unsigned inputSize, const float* output,
                   unsigned outSize) override;
  
  std::vector<float> param;
};

class Conv1DLayer : public Layer {
 public:
  Conv1DLayer(unsigned window, unsigned stride, unsigned pad,
              unsigned numOutput)
      : Layer(window, numOutput), _stride(stride), _pad(pad) {
    //+1 for weight for bias input.
    _weights.Allocate( window + 1, numOutput);
  }
  //output is organized such that for each output pixel, the 
  //value convolved with each filter are stored next to each other.
  virtual int f_imp(const float* input, unsigned inputSize, float* output,
                unsigned outSize) override;
  // derivative w.r.t input. num input cols x num output rows
  virtual int dodx(Array2Df& dx, const Array2Df& dody) override;
  // derivative w.r.t weight. num weights cols x num output rows.
  virtual int dodw(Array2Df& dw, const Array2Df& dody, unsigned oi) override;
  void UpdateCache(const float* input, unsigned inputSize, const float* output,
                   unsigned outSize) override;

  unsigned NumOutput(unsigned inputSize) const override {
    return _numNodes * NumWindows(inputSize);
  }

  unsigned NumWindows(unsigned inputSize) const {
    unsigned window = _numInput;
    if (inputSize + 2 * _pad < window) {
      return 1;
    }
    return (inputSize + 2 * _pad - window) / _stride + 1;
  }

  unsigned _pad = 0;
  unsigned _stride = 1;

};

class ANN {
 public:
  ANN(unsigned inputSize) : _inputSize(inputSize) {}
  void SaveWeights(std::ostream& out) const;
  void LoadWeights(std::istream& in);
  int f(const float* input, unsigned inputSize, float* output,
        unsigned outSize);
  /// <summary>
  /// need to call f() first, so that input
  /// and intermediate values of each layer are computed.
  /// </summary>
  /// <param name="dw">output weight gradient for each layer</param>
  /// <param name="dOut">derivative of output w.r.t objective func</param>  
  /// <returns>0 on success</returns>
  int dfdw(std::vector<Array2Df> & dw, const Array2Df & dOut);

  void AddLayer(std::shared_ptr<Layer> layer) { _layers.push_back(layer); }
  //weights of all layers ordered from input to output.
  std::vector<float> GetWeights() const;
  void SetWeights(const std::vector<float>& weights);

  std::vector<Array2Df> GetWeightsLayers() const;
  void SetWeightsLayers(const std::vector<Array2Df>& weights);

  std::vector<std::shared_ptr<Layer> > _layers;
  unsigned _inputSize = 1;
};

#endif