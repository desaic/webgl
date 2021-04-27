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
  
  //GridNodeLeaf() :origin(0) {}
  void SetValue(uint8_t x, uint8_t y, uint8_t z, void* valp) override {
    val.SetValue(GetLinIdx(x, y, z), *(ValueT*)valp);
  }
  
  void AddValue(uint8_t x, uint8_t y, uint8_t z, void* valp) override {
    val.AddValue(GetLinIdx(x, y, z), *(ValueT*)valp);
  }

  bool HasValue(uint8_t x, uint8_t y, uint8_t z) override
  {
    unsigned linIdx = GetLinIdx(x, y, z);
    return val.HasValue(linIdx);
  }

  void GetValue(uint8_t x, uint8_t y, uint8_t z, void* valp) override
  {
    *(ValueT*)(valp) = val.GetValue(GetLinIdx(x, y, z));
  }

  unsigned GetNumValues() const override 
  { 
    return val.GetNumValues();
  }

  GridNode* GetChild(uint8_t x, uint8_t y, uint8_t z) override {
    return nullptr;
  }

  ///doesn't do anything.
  void AddChild(GridNode* child, uint8_t x, uint8_t y, uint8_t z) {}

  //is there value for cell i
  SparseSet<ValueT> val;

  ///16 bit for each of x y z coordinate.
  //uint64_t origin;  
};
