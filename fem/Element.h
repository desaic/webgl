#ifndef ELEMENT_H
#define ELEMENT_H

#include "FixedArray.h"

class Element {
 public:
  Element(unsigned numVerts = 4) : v(numVerts) {}
  ~Element() {}

  unsigned operator[](unsigned i) const { return v[i]; }
  unsigned& operator[](unsigned i) { return v[i]; }

 protected:
  FixedArray<unsigned int> v;
};

class HexElement : public Element {
 public:
  HexElement() : Element(8) {}
};

class TetElement : public Element {
 public:
  TetElement() : Element(4) {}
};

#endif