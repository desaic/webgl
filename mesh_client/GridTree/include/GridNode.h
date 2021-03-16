#pragma once

class GridNode {
public:

  GridNode() :log2BF(3),origin(0) {}

  virtual void AddValue(unsigned x, unsigned y, unsigned z, void* valp)=0;

  ///\param valp pointer to a value.
  virtual void SetValue(unsigned x, unsigned y, unsigned z, void* valp)=0;
  
  virtual void GetValue(unsigned x, unsigned y, unsigned z, void* valp) = 0;
  
  virtual bool HasValue(unsigned x, unsigned y, unsigned z) = 0;

  virtual unsigned GetNumValues() const { return 0; }

  virtual void SetLog2BF(unsigned char l) { log2BF = l; }

  ///Get child node by local x y z index. returns nullptr if child does not exist.
  virtual GridNode* GetChild(unsigned x, unsigned y, unsigned z) = 0;

  virtual void AddChild(GridNode* child, unsigned x, unsigned y, unsigned z) = 0;

  ///obviously not thread safe.
  ///\return a list of GridNode * .
  virtual GridNode** GetChildren() { return nullptr; }

  virtual unsigned GetNumChildren() const { return 0; }

  virtual ~GridNode() {}

  virtual void SetOrigin(unsigned x, unsigned y, unsigned z) {
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
