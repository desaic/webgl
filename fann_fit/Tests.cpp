#include "ann.h"
#include "Array2D.h"
#include "ArrayUtil.h"
#include <iostream>
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
  std::vector<float> y = {3, 5};
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
  for (size_t j = 0; j < conv1d._weights.Rows(); j++) {
    for (size_t i = 0; i < conv1d._weights.Cols(); i++) {
      conv1d._weights(i, j) = j * 21 + i;
    }
  }
  for (size_t i = 0; i < x.size(); i++) {
    x[i] = i + 1;
  }

  for (size_t i = 0; i < dody.Cols(); i++) {
    dody(i, 0) = i + 1;
  }
  conv1d.F(x.data(), x.size(), y.data(), y.size());
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

void TestAddTimes() {
  std::vector<Array2Df> mats(2);
  std::vector<Array2Df> dw(2);
  size_t count = 0;
  for (size_t i = 0; i < mats.size(); i++) {
    mats[i].Allocate(i + 2, i + 3);
    dw[i].Allocate(i + 2, i + 3);
    for (size_t j = 0; j < mats[i].GetData().size(); j++) {
      mats[i].GetData()[j] = count;
      dw[i].GetData()[j] = 0.1f * count;
      count++;
    }
  }
  AddTimes(mats, dw, 0.1f);
  for (size_t i = 0; i < mats.size(); i++) {
    for (size_t row = 0; row < mats[i].Rows(); row++) {
      for (size_t col = 0; col < mats[i].Cols(); col++) {
        std::cout << mats[i](col, row) << " ";
      } 
      std::cout << "\n";
    }

    std::cout << "\n";
  }
  std::cout << "\n";
}