#pragma once

#include <vector>

#include "Array2D.h"
#include "cs.h"
/// c++ interface for csparse struct.
/// not copyable because of pointers and laziness.
class CSparseMat {
 public:
  /// <summary>
  /// resize vectors and initialize pointers.
  /// </summary>
  /// <param name="row">num rows</param>
  /// <param name="col">num cols</param>
  /// <param name="nz">num nonzeros</param>
  void Allocate(unsigned rows, unsigned cols, unsigned nz);

  unsigned Rows() const { return csMat.m; }
  unsigned Cols() const { return csMat.n; }
  size_t NNZ() const { return csMat.nzmax; }
  /// <summary>
  /// Assumes  matrix is square .
  /// </summary>
  /// <returns></returns>
  std::vector<float> Diag() const;
  std::vector<float> MultSimple(const float* v) const;
  void MultSimple(const double* v, double * b) const;
  // for debugging
  int ToDense(float* matOut) const;

  // where does column i start in rowIdx and vals.
  // size = cols+1.
  // fist element is always 0. last element is always nz.
  std::vector<csi> colStart;
  // indices of non-zero rows for each column.
  // size = nz.
  std::vector<csi> rowIdx;
  // csparse uses double.
  // size = nz.
  std::vector<double> vals;

  static const size_t MAX_DENSE_MATRIX_ENTRIES = 100000000;

  // csparse struct
  struct cs_sparse csMat = {};

};

/// The diagonal entries in K must be present
/// otherwise will not work properly.
void FixDOF(std::vector<bool> & fixedDOF, CSparseMat & K, float scale);

/// <summary>
/// 
/// </summary>
/// <param name="K"></param>
/// <param name="x"></param>
/// <param name="b"></param>
/// <param name="maxIter"></param>
/// <returns>positive number for number of iterations. negative number for
/// errors.</returns>
int CG(const CSparseMat& K, std::vector<double>& x, const std::vector<double>& b,
       int maxIter = 100);