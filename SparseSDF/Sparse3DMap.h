#ifndef SPARSE_3D_MAP_H
#define SPARSE_3D_MAP_H
#include "Array3D.h"
#include "SparseNode4.h"

// TIndex type of index, usually unsigned or size_t
// TData type of stored data.
template <typename TIndex>
class Sparse3DMap {
 public:
  Sparse3DMap() : count_(1) {}
  void Allocate(unsigned x, unsigned y, unsigned z);
  /// <summary>
  /// Does not create new entry if already exists.
  /// Does not work after Compress().
  /// </summary>
  /// <returns>Index of added item.</returns>
  TIndex AddDense(unsigned x, unsigned y, unsigned z);

  /// <summary>
  /// Removes spaces reserved for empty cells.
  /// </summary>
  void Compress();

  /// <returns>0 if child does not exist</returns>
  TIndex Get(unsigned x, unsigned y, unsigned z) const;
  
  SparseNode4<TIndex>& GetSparseNode4(unsigned x, unsigned y, unsigned z);
  const SparseNode4<TIndex>& GetSparseNode4(unsigned x, unsigned y, unsigned z)const;
  bool HasDense(unsigned x, unsigned y, unsigned z) const;
  bool HasSparse(unsigned x, unsigned y, unsigned z)const;

  Vec3u CoarseGridSize() const { return grid_.GetSize(); }
 private:
  // sparse map from 3D index to linear index.
  Array3D<SparseNode4<TIndex> > grid_;
  // 0 is reserved for empty cells.
  TIndex count_ = 1;
};

#endif