#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
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
      if (std::strncmp(line.c_str(), "v8_0305_x", 9) == 0) {
        std::cout << data.size() << "\n";
      }
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

bool compareByY0(const DataPoint& a, const DataPoint& b) {
  return a.y[0] < b.y[0];  // Sort in ascending order by y
}

//make each curve of length 20 zero mean.
void CenterEachCurve(std::vector<DataPoint> & dp) { 
  const unsigned CURVE_LEN = 20;
  for (size_t i = 0; i < dp.size(); i++) {
    for (size_t c0 = 0; c0 < dp[i].x.size(); c0 += CURVE_LEN) {
      std::vector<float> curve(dp[i].x.begin() + c0,
                               dp[i].x.begin() + c0 + CURVE_LEN);
      float mean = Sum(curve);
      mean /= CURVE_LEN;
      for (auto& f : curve) {
         f-= mean;
      }
      for (size_t j = 0; j < CURVE_LEN; j++) {
         dp[i].x[c0 + j] = curve[j];
      }
    }
  }
}

std::vector<DataPoint> ShiftX(const std::vector<DataPoint>& dp, int offset) {
  const unsigned CurveLen = 20;
  std::vector<DataPoint> out(dp.size());
  for (size_t i = 0; i < dp.size(); i++) {
    out[i] = dp[i];
    for (size_t c0 = 0; c0 < dp[i].x.size(); c0+=CurveLen) {
      for (size_t j = 0; j < CurveLen; j++) {
         int dst_j = int(c0 + j);
         int src_j = dst_j - offset;
         if (src_j < int(c0)) {
           src_j = int(c0);
         }
         if (src_j >= int(c0 + CurveLen)) {
           src_j = int(c0 + CurveLen) - 1;
         }
         out[i].x[dst_j] = dp[i].x[src_j];
      }
    }
  }
  return out;
}

void MakeOneDenseLayer(ANN& ann) { 
  const unsigned numFeat = 16; 
  std::shared_ptr<DenseLinearLayer> dense =
      std::make_shared<DenseLinearLayer>(numFeat, 1);  
  ann.AddLayer(dense);
}

std::vector<DataPoint> ExtractValleyDepth(const std::vector<DataPoint> & data) {
  std::vector<DataPoint> out(data.size());
  const unsigned CURVE_LEN = 20;

  const unsigned WINDOW = 3;
  for (size_t i = 0; i < data.size(); i++) {
    out[i].y = data[i].y;
    out[i].x.resize(16, 0);
    for (size_t c0 = 0; c0 < data[i].x.size(); c0 += CURVE_LEN) {
      float minVal = 0;
      for (size_t j = WINDOW; j < CURVE_LEN- WINDOW; j++) {
         minVal = std::min(data[i].x[c0 + j], minVal);
      }
      float leftSum = 0, rightSum = 0;
      for (size_t j = 0; j < WINDOW; j++) {
        leftSum += data[i].x[c0+j];
         rightSum += data[i].x[c0 + CURVE_LEN - 1 - j];
      }
      leftSum /= WINDOW;
      rightSum /= WINDOW;
      out[i].x[c0 / CURVE_LEN] = std::min(leftSum -minVal, rightSum - minVal);
    }
  }
  return out;
}

int main(int argc, char* argv[]) {
  ANNTrain train;
  ReadDataSet(train._data);
  std::vector<DataPoint> ds_left=ShiftX(train._data, -1);
  std::vector<DataPoint> ds_right = ShiftX(train._data, 1);
  train._data.insert(train._data.end(), ds_left.begin(), ds_left.end());
  train._data.insert(train._data.end(), ds_right.begin(), ds_right.end());
  CenterEachCurve(train._data);
  std::vector<DataPoint> oldData = train._data;
  train._data = ExtractValleyDepth(oldData);
  train._ann = std::make_shared<ANN>(16);
  MakeOneDenseLayer(*train._ann);

  //auto& points = train._data;
  //std::sort(points.begin(), points.end(), compareByY0);
  
  //unsigned inputSize = train._data[0].x.size();
  //train._ann = std::make_shared<ANN>(inputSize);
  //MakeDiamondNetwork(*train._ann);
  //std::ifstream in("F:/dump/dia_2.model");
  //train._ann->LoadWeights(in);
  //std::vector<float> weights = train._ann->GetWeights();
  //for (size_t i = 0; i < weights.size(); i++) {
  //  weights[i] = (i % 97) / 100.0f;
  //}
  //for (size_t i = 0; i < 7; i++) {
  //  weights[i + 6] = 0;
  //  weights[i + 33] = 0;
  //}
  //train._ann->SetWeights(weights);
  
  const int NUM_STEPS = 1000;
  for (int i = 0; i < NUM_STEPS; i++) {
    int ret = train.GradStep();
    if (ret < 0) {
      std::cout << "can't reduce objective\n";
      break;
    }
  }
  std::ofstream out("F:/dump/lin_3.model");
  train._ann->SaveWeights(out);
  for (size_t i = 0; i < train._data.size(); i++) {
    const DataPoint& d = train._data[i];
    float out = 0;
    train._ann->f(d.x.data(), d.x.size(), &out, 1);
    std::cout << "label & pred: " << d.y[0] << " " << out << "\n";
  }
  return 0;
}
