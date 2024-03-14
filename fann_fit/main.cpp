#include <fstream>
#include <iostream>
#include <sstream>

#include "ann.h"
#include "ann_train.h"

void ReadDataSet(std::vector<DataPoint>& data) {
  std::string dataRoot = "H:/ml/diamond_offset/data/";
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

int main(int argc, char* argv[]) {
  std::vector<DataPoint> data;
  ReadDataSet(data);
  ANN ann;
  MakeDiamondNetwork(ann);
  std::vector<float> weights = ann.GetWeights();
  for (size_t i = 0; i < weights.size(); i++) {
    weights[i] = (i % 97) / 100.0f;
  }
  ann.SetWeights(weights);
  return 0;
}
