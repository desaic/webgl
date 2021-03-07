#pragma once

#include "GridNode.h"
#include "BitArray.h"
#include "SparseSet.h"

template <typename ValueT>
class GridNodeInternal : public GridNode {
public:
  GridNodeInternal() {}

  ///get linear index
  unsigned GetLinIdx(unsigned x, unsigned y, unsigned z) 
  {
    unsigned idx = (x << (2 * log2BF)) | (y << log2BF) | z;
    return idx;
  }

  ///assumes that the spot exists. otherwise use AddValue.
  void SetValue(unsigned x, unsigned y, unsigned z, void* valp) override 
  {
    unsigned linIdx = GetLinIdx(x, y, z);
    val.SetValue(linIdx, *(ValueT*)(valp));
  }

  ///assumes that the spot does not exist. otherwise use SetValue.
  void AddValue(unsigned x, unsigned y, unsigned z, void* valp) override 
  {
      unsigned linIdx = GetLinIdx(x, y, z);
      val.AddValue(linIdx, *(ValueT*)valp);
  }

  bool HasValue(unsigned x, unsigned y, unsigned z) override
  {
    unsigned linIdx = GetLinIdx(x, y, z);
    return val.HasValue(linIdx);
  }

  void GetValue(unsigned x, unsigned y, unsigned z, void* valp) override
  {
    unsigned linIdx = GetLinIdx(x, y, z);
    *(ValueT*)valp = val.GetValue(linIdx);
  }
  
  unsigned GetNumValues() const override{ return val.GetNumValues(); }

  GridNode* GetChild(unsigned x, unsigned y, unsigned z) override
  {
    unsigned linIdx = GetLinIdx(x, y, z);
    return children.GetValue(linIdx);
  }

  void AddChild(GridNode* child, unsigned x, unsigned y, unsigned z) override
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
    val.Allocate(numEntries);
  }

  void Free() {
    for (size_t i = 0; i < children.val.size(); i++) {
      delete children.val[i];
    }
    children.val.clear();
    val.val.clear();
  }

  ~GridNodeInternal() {
    Free();
  }

  SparseSet<GridNode*> children;
  SparseSet<ValueT> val;
};
