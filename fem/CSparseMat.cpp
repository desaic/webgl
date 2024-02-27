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