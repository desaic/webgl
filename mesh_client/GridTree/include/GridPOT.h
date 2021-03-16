#pragma once
///grid size is always power of 2
template <typename ValueT>
struct Grid3DPOT {
  std::vector<ValueT> data;
  unsigned log2Size;
  const ValueT & operator()(size_t x, size_t y, size_t z)const {
    size_t idx = (x << (2 * log2Size)) | (y << log2Size) | z;
    return data[idx];
  }

  ValueT& operator()(size_t x, size_t y, size_t z) {
    size_t idx = (x << (2 * log2Size)) | (y << log2Size) | z;
    return data[idx];
  }

  void Allocate(unsigned Log2Size) {
    log2Size = Log2Size;
    data.resize( 1LL << (3 * Log2Size) );
  }

};
