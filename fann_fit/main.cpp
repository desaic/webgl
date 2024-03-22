#include <fstream>
#include <iostream>
#include <sstream>

#include "ann.h"
#include "ann_train.h"
#include "ArrayUtil.h"

#include <chrono>
#include <thread>

void ReadDataSet(std::vector<DataPoint>& data) {
  std::string dataRoot = "H:/ml/diamond_offset/train/";
  std::string folders[7] = {dataRoot + "v7_0213/",   dataRoot + "v7_0213_2/",
                            dataRoot + "v7_0724/",   dataRoot + "v7_1012_1/",
                            dataRoot + "v7_1012_2/", dataRoot + "v8_0305/",
                            dataRoot + "v8_0306/"};
  for (size_t i = 0; i < 7; i++) {
    std::string dataFile = folders[i] + "/train.txt";
    std::ifstream in(dataFile);
    DataPoint dp;
    std::string line;
    while (std::getline(in, line)) {
      std::stringstream iss(line);
      dp.Read1D(iss);
      data.push_back(dp);
    }
  }
}

// compute module offsets from diamond edges
void MakeDiamondNetwork(ANN& ann) {
  const unsigned CURVE_WIDTH = 20;
  unsigned numFilters = 3;
  std::shared_ptr<Conv1DLayer> convLayer =
      std::make_shared<Conv1DLayer>(CURVE_WIDTH, CURVE_WIDTH, 0, numFilters);
  std::shared_ptr<ActivationLayer> act1 =
      std::make_shared<ActivationLayer>(48, ActivationLayer::LRelu);
  ann.AddLayer(convLayer);
  ann.AddLayer(act1);
  std::shared_ptr<Conv1DLayer> conv2= std::make_shared<Conv1DLayer>(6,6,0,2);
  ann.AddLayer(conv2);
  std::shared_ptr<ActivationLayer> act2 =
      std::make_shared<ActivationLayer>(16, ActivationLayer::LRelu);
  std::shared_ptr<DenseLinearLayer> dense =
      std::make_shared<DenseLinearLayer>(16, 1);
  ann.AddLayer(act2);
  ann.AddLayer(dense);
}

void TestMVProd() {
  Array2Df mat(4, 2);
  mat(0, 0) = 1;
  mat(1, 0) = 2;
  mat(2, 0) = 3;
  mat(3, 0) = 1;
  mat(0, 1) = 5;
  mat(1, 1) = 8;
  mat(2, 1) = 13;
  mat(3, 1) = 1;
  std::vector<float> x = {2, 4, 6};
  std::vector<float> y = {3,5};
  std::vector<float> prodx(2), prody(3);
  MVProd(mat, x.data(), x.size(), prodx.data(), prodx.size());
  MTVProd(mat, y.data(), y.size(), prody.data(), prody.size());
  for (auto f : prodx) {
    std::cout << f << " ";
  }
  std::cout << "\n";

  for (auto f : prody) {
    std::cout << f << " ";
  }
  std::cout << "\n";
}

void TestConvGrad() {
  Conv1DLayer conv1d(20, 20, 0, 3);
  std::vector<float> x(100), y(15);
  Array2Df dody(15, 1);
  for (size_t j = 0; j < conv1d._weights.Rows();j++) {
    for (size_t i = 0; i < conv1d._weights.Cols();i++) {
      conv1d._weights(i, j) = j*21+i;
    }
  }
  for (size_t i = 0; i < x.size(); i++) {
    x[i] = i + 1;
  }
  
  for (size_t i = 0; i < dody.Cols(); i++) {
    dody(i, 0) = i + 1;
  }
  conv1d.F(x.data(),x.size(), y.data(), y.size());
  Array2Df dx;
  conv1d.dodx(dx, dody);
  Array2Df dw;
  conv1d.dodw(dw, dody, 0);
  for (size_t i = 0; i < dw.Rows(); i++) {
    for (size_t j = 0; j < dw.Cols(); j++) {
      std::cout << dw(j, i) << " ";
    }
    std::cout << "\n";
  }
}

int main(int argc, char* argv[]) {
  TestConvGrad();
  ANNTrain train;
  ReadDataSet(train._data);
  unsigned inputSize = train._data[0].x.size();
  train._ann = std::make_shared<ANN>(inputSize);
  MakeDiamondNetwork(*train._ann);
  std::vector<float> weights = train._ann->GetWeights();
  for (size_t i = 0; i < weights.size(); i++) {
    weights[i] = (i % 97) / 100.0f;
  }
  train._ann->SetWeights(weights);
  const DataPoint& dp = train._data[0];
  float y = 0;
  train.CenterInput();
  train._ann->f(dp.x.data(), dp.x.size(), &y, 1);
  int ret = train.GradStep();
  return 0;
}
