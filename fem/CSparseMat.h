#pragma once
#include <vector>

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

  // where does column i start in rowIdx and vals.
  // size = cols+1. last element is always nz
  std::vector<csi> colStart;
  // indices of non-zero rows for each column.
  // size = nz.
  std::vector<csi> rowIdx;
  // csparse uses double.
  // size = nz.
  std::vector<double> vals;

  // csparse struct
  struct cs_sparse csMat = {};
};