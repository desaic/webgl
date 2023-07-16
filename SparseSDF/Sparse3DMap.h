#ifndef SPARSE_3D_MAP_H
#define SPARSE_3D_MAP_H
#include "Array3D.h"
#include "SparseNode4.h"

// TIndex type of index, usually unsigned or size_t
// TData type of stored data.
template <typename TIndex, typename TData>
class Sparse3DMap {
 public:
  Sparse3DMap() : data(1) {}
  // sparse map from 3D index to linear index.
  Array3D<SparseNode4<TIndex> > grid;
  // data 0 is reserved for empty cells.
  std::vector<TData> data;
};

#endif