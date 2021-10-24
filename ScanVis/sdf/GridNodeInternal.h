#pragma once

#include "GridNode.h"
#include "BitArray.h"
#include "SparseSet.h"

template <typename ValueT>
class GridNodeInternal : public GridNode {
public:
  GridNodeInternal() {}

  ///assumes that the spot exists. otherwise use AddValue.
  void SetValue(uint8_t x, uint8_t y, uint8_t z, void* valp) override
  {    
  }

  ///assumes that the spot does not exist. otherwise use SetValue.
  void AddValue(uint8_t x, uint8_t y, uint8_t z, void* valp) override
  {
  }

  bool HasValue(uint8_t x, uint8_t y, uint8_t z) override
  {
    return false;
  }

  void GetValue(uint8_t x, uint8_t y, uint8_t z, void* valp) override
  {
  }
  
  unsigned GetNumValues() const override{ return 0; }

  GridNode* GetChild(uint8_t x, uint8_t y, uint8_t z) override
  {
    unsigned linIdx = GetLinIdx(x, y, z);
    return children.GetValue(linIdx);
  }

  void AddChild(GridNode* child, uint8_t x, uint8_t y, uint8_t z) override
  {
    unsigned linIdx = GetLinIdx(x, y, z);
    children.AddValue(linIdx, child);
  }

  GridNode** GetChildren() { return children.val.data(); }

  unsigned GetNumChildren() const override{ return children.GetNumValues(); }

  void Allocate() 
  {
    size_t numEntries = 1ULL << (3 * log2BF);
    children.Allocate(numEntries);
  }

  void Free() {
    for (size_t i = 0; i < children.val.size(); i++) {
      delete children.val[i];
    }
    children.val.clear();
  }

  ~GridNodeInternal() {
    Free();
  }

  SparseSet<GridNode*> children;
};
