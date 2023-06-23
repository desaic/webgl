#pragma once

#include <cstdint>

class GridNode {
public:

  GridNode() :log2BF(3),origin(0) {}

  unsigned GetLinIdx(uint8_t x, uint8_t y, uint8_t z)
  {
    unsigned idx = (unsigned(x) << (2 * log2BF)) | (unsigned(y) << log2BF) | z;
    return idx;
  }

  virtual void AddValue(uint8_t x, uint8_t y, uint8_t z, void* valp)=0;

  ///\param valp pointer to a value.
  virtual void SetValue(uint8_t x, uint8_t y, uint8_t z, void* valp)=0;
  
  virtual void GetValue(uint8_t x, uint8_t y, uint8_t z, void* valp) = 0;
  
  virtual bool HasValue(uint8_t x, uint8_t y, uint8_t z) = 0;

  virtual unsigned GetNumValues() const { return 0; }

  virtual void SetLog2BF(uint8_t l) { log2BF = l; }

  ///Get child node by local x y z index. returns nullptr if child does not exist.
  virtual GridNode* GetChild(uint8_t x, uint8_t y, uint8_t z) = 0;

  virtual void AddChild(GridNode* child, uint8_t x, uint8_t y, uint8_t z) = 0;

  ///obviously not thread safe.
  ///\return a list of GridNode * .
  virtual GridNode** GetChildren() { return nullptr; }

  virtual unsigned GetNumChildren() const { return 0; }

  virtual ~GridNode() {}

  virtual void SetOrigin(uint8_t x, uint8_t y, uint8_t z) {
    origin = 0;
    origin = ((uint64_t(x) << 40) | (uint64_t(y) << 20) | z);
  }

  virtual void GetOrigin(unsigned& x, unsigned& y, unsigned& z) {
    x = unsigned(origin >> 40);
    y = unsigned(origin >> 20) & 0xfffff;
    z = unsigned(origin) & 0xfffff;
  }

  ///size along 1 axis.
  virtual unsigned GetGridSize()const { return 1 << log2BF; }

protected:
  
  ///log2 branching factor
  unsigned char log2BF;
  uint64_t origin;

};
