#ifndef SPARSE_NODE_4_H
#define SPARSE_NODE_4_H

#include "BitOps.h"

// sparse node for a 4x4x4 block
template <typename T>
struct SparseNode4 {
  static const unsigned NUM_CHILDREN = 64;
  static const unsigned GRID_SIZE = 4;
  SparseNode4() {}

  ~SparseNode4() {
    if (children != nullptr) {
      delete[] children;
    }
  }

  void AddChildrenDense(T initVal) {
    if (!HasChildren()) {
      AllocateChildren();
    }
    for (unsigned i = 0; i < NUM_CHILDREN; i++) {
      children[i] = initVal;
    }
  }

  /// do not use after compression. assumes all 64 children are allocated.
  /// does not create new child if child is already allocated.
  /// @return address of child
  T* AddChildDense(unsigned x, unsigned y, unsigned z) {
    unsigned linearIdx = LinearIdx(x, y, z);
    uint64_t childMask = (1ull << linearIdx);
    bool hasChild = mask & childMask;
    if (!hasChild) {
      mask |= childMask;
    }
    return &children[linearIdx];
  }

  const T* GetChildDense(unsigned x, unsigned y, unsigned z) const {
    unsigned linearIdx = LinearIdx(x, y, z);
    return &children[linearIdx];
  }

  // done with adding children. AddChild no longer works.
  // only allows GetChild.
  void Compress() {
    unsigned childCount = CountOn(mask);
    if (childCount == 0) {
      return;
    }
    T* newChildren = new T[childCount];
    unsigned count = 0;
    for (unsigned i = 0; i < NUM_CHILDREN; i++) {
      uint64_t childMask = (1ull << i);
      bool hasChild = mask & childMask;
      if (hasChild) {
        newChildren[count] = children[i];
        count++;
      }
    }
    delete[] children;
    children = newChildren;
  }

  void AllocateChildren() { children = new T[NUM_CHILDREN]; }
  unsigned LinearIdx(unsigned x, unsigned y, unsigned z) const {
    unsigned i = (z * GRID_SIZE + y) * GRID_SIZE + x;
    return i;
  }
  bool HasChildren() const { return children != nullptr; }

  bool HasChild(unsigned x, unsigned y, unsigned z) const {
    if (children == nullptr) {
      return false;
    }
    unsigned linearIdx = LinearIdx(x, y, z);
    return mask & (1ull << linearIdx);    
  }
  
  bool HasChild(unsigned linearIdx) const {
    if (children == nullptr) {
      return false;
    }
    return mask & (1ull << linearIdx);
  }
  
  // does not work before compression.
  const T* GetChild(unsigned x, unsigned y, unsigned z) const {
    
    unsigned linearIdx = LinearIdx(x, y, z);
    bool hasChild = mask & (1ull << linearIdx);
    if (!hasChild) {
      return nullptr;
    }
    unsigned childIdx = CountOn(mask & ((1ull << linearIdx) - 1));
    return &children[childIdx];
  }

  // does not work before compression.
  T* GetChild(unsigned x, unsigned y, unsigned z) {
    unsigned linearIdx = LinearIdx(x, y, z);
    bool hasChild = mask & (1ull << linearIdx);
    if (!hasChild) {
      return nullptr;
    }
    unsigned childIdx = CountOn(mask & ((1ull << linearIdx) - 1));
    return &children[childIdx];
  }

  uint64_t mask = 0;
  // array of child cells.
  T* children = nullptr;
};
#endif