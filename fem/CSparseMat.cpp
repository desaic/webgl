#include "CSparseMat.h"

void CSparseMat::Allocate(unsigned rows, unsigned cols, unsigned nz) {
  csMat.nzmax = nz;
  csMat.m = rows;
  csMat.n = cols;
  colStart.resize(cols + 1, 0);
  vals.resize(nz, 0);
  rowIdx.resize(nz, 0);
  // column pointers (size n+1) or col indices (size nzmax)
  csMat.p = colStart.data();
  // row indices, size nzmax
  csMat.i = rowIdx.data();
  // numerical values, size nzmax
  csMat.x = vals.data();
  // #of entries in triplet matrix, -1 for compressed - col
  csMat.nz = -1;
}

std::vector<float> CSparseMat::MultSimple(const float* v) const {
  std::vector<float> prod(Rows(), 0);
  for (unsigned col = 0; col < Cols(); col++) {
    float vcol = v[col];
    size_t st = colStart[col];
    size_t nnz = colStart[col + 1] - colStart[col];
    for (unsigned i = 0; i < nnz; i++) {
      unsigned row = rowIdx[st + i];
      float val = vals[st + i];
      prod[row] += vcol * val;
    }
  }
  return prod;
}

int CSparseMat::ToDense(float* matOut) const {
  if (matOut == nullptr) {
    return -1;
  }
  size_t matSize = Rows() * size_t(Cols());
  if (matSize == 0) {
    return -2;
  }
  if (matSize > MAX_DENSE_MATRIX_ENTRIES) {
    return -3;
  }
  for (size_t i = 0; i < matSize; i++) {
    matOut[i] = 0;
  }
  for (unsigned col = 0; col < Cols(); col++) {
    size_t st = colStart[col];
    size_t numRows = colStart[col + 1] - st;
    for (unsigned i = 0; i < numRows; i++) {
      size_t row = rowIdx[st + i];
      double val = vals[st + i];
      matOut[row * Rows() + col] = val;
    }
  }
  return 0;
}

void FixDOF(std::vector<bool>& fixedDOF, CSparseMat& K, float scale) {

  unsigned rows = K.Rows();
  unsigned cols = K.Cols();
  for (unsigned col = 0; col < cols; col++) {
    size_t st = K.colStart[col];
    size_t numRows = K.colStart[col+1] - st;
    if (fixedDOF[col]) {
      for (unsigned i = 0; i < numRows; i++) {
        unsigned row = K.rowIdx[st + i];
        if (row == col) {
          K.vals[st + i] = scale;
        } else {
          K.vals[st + i] = 0;
        }
      }
    } else {
      for (unsigned i = 0; i < numRows; i++) {
        unsigned row = K.rowIdx[st + i];
        if (fixedDOF[row]) {
          K.vals[st + i] = 0;
        }
      }
    }
  }
}