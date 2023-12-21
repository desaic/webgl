#ifndef ELEMENT_H
#define ELEMENT_H

#include "FixedArray.h"
#include <vector>
class Element {
 public:
  Element(unsigned numVerts = 4) : _v(numVerts) {}
  ~Element() {}

  unsigned operator[](unsigned i) const { return _v[i]; }
  unsigned& operator[](unsigned i) { return _v[i]; }

  virtual unsigned NumEdges() const { return 0; }
  virtual void Edge(unsigned ei, unsigned& v1, unsigned& v2) const {}
  virtual unsigned NumFaces() const { return 0; }
  virtual std::vector<unsigned> Face(unsigned fi) const {
    return std::vector<unsigned>();
  }
  virtual unsigned NumVerts() const { return _v.size(); }
 protected:
  FixedArray<unsigned int> _v;
};

//z     3        7
//   1        5
//        y
//      2        6
//   0        4     x
class HexElement : public Element {
 public:
  HexElement() : Element(8) {}
  unsigned NumEdges() const override{ return 12; }
  void Edge(unsigned ei, unsigned& v1, unsigned& v2) const override;
  unsigned NumFaces() const override { return 6; }
  std::vector<unsigned> Face(unsigned fi) const override;
};

// z    
//    3 
//         y
//       2   
//    0       1   x
class TetElement : public Element {
 public:
  TetElement() : Element(4) {}
  unsigned NumEdges() const override{ return 6; }
  void Edge(unsigned ei, unsigned& v1, unsigned& v2) const override;
  unsigned NumFaces() const override { return 4; }
  std::vector<unsigned> Face(unsigned fi) const override;
};

#endif