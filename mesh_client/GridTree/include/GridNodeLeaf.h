#pragma once

#include "GridNode.h"
#include "SparseSet.h"

template <typename ValueT>
class GridNodeLeaf : public GridNode {
public:

  void Allocate() {
    size_t numEntries = 1ULL << (3 * log2BF);
    val.Allocate(numEntries);
  }

  ///get linear index
  unsigned GetLinIdx(unsigned x, unsigned y, unsigned z)
  {
    unsigned idx = (x << (2 * log2BF)) | (y << log2BF) | z;
    return idx;
  }
  
  //GridNodeLeaf() :origin(0) {}
  void SetValue(unsigned x, unsigned y, unsigned z, void* valp) override {
    val.SetValue(GetLinIdx(x, y, z), *(ValueT*)valp);
  }
  
  void AddValue(unsigned x, unsigned y, unsigned z, void* valp) override {
    val.AddValue(GetLinIdx(x, y, z), *(ValueT*)valp);
  }

  bool HasValue(unsigned x, unsigned y, unsigned z) override
  {
    unsigned linIdx = GetLinIdx(x, y, z);
    return val.HasValue(linIdx);
  }

  void GetValue(unsigned x, unsigned y, unsigned z, void* valp) override
  {
    *(ValueT*)(valp) = val.GetValue(GetLinIdx(x, y, z));
  }

  unsigned GetNumValues() const override 
  { 
    return val.GetNumValues();
  }

  GridNode* GetChild(unsigned x, unsigned y, unsigned z) override {
    return nullptr;
  }

  ///doesn't do anything.
  void AddChild(GridNode* child, unsigned x, unsigned y, unsigned z) {}

  //is there value for cell i
  SparseSet<ValueT> val;

  ///16 bit for each of x y z coordinate.
  //uint64_t origin;  
};
