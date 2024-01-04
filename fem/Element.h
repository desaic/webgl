#ifndef ELEMENT_H
#define ELEMENT_H

#include <vector>

#include "Matrix3f.h"
#include "FixedArray.h"
#include "Quadrature.h"
#include "Vec3.h"

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
  
  virtual Vec3f GetDisp(const Vec3f& p, const std::vector<Vec3f>& X,
                        const std::vector<Vec3f>& x);

  ///@param p natural coordinates.
  ///@return interpolation weight for each dof.
  virtual std::vector<float> shapeFun(const Vec3f& p) const = 0;

  ///@param ii index of basis function.
  ///@param xx point in natural coordinates.
  ///@param X global array of rest positions.
  virtual Vec3f shapeFunGrad(int ii, const Vec3f& xx) const = 0;

  /// <summary>
  /// deformation gradient at a 2-point gaussian quadrature point.
  /// </summary>
  /// <param name="qi">index of quadrature point</param>
  /// <param name="X">vertices of ElementMesh</param>
  /// <param name="x"></param>
  /// <returns></returns>
  virtual Matrix3f DefGradGauss2(int qi, const std::vector<Vec3f>& X,
                                 const std::vector<Vec3f>& x) const {
    return Matrix3f();
  }
  virtual Vec3f ShapeFunGradGauss2(unsigned qi, unsigned vi) const {
    return Vec3f(0, 0, 0);
  }
  // called by ElementMesh::InitElements()
  virtual void InitJacobian(QUADRATURE_TYPE qtype,
                            const std::vector<Vec3f>& X) {}
  float DetJ(unsigned qi) const { return _detJ[qi]; }
  const Matrix3f& Jinv(unsigned qi) const { return _Jinv[qi]; }
 protected:
  FixedArray<unsigned int> _v;

  ///@brief inverse of material coordinate with respect to natural coordinate
  /// derivative.
  /// initialized by InitJacobian()
  std::vector<Matrix3f> _Jinv;
  
  ///@brief material space volume at each quadrature point.
  std::vector<float> _detJ;
};

// z     3        7
//    1        5
//         y
//       2        6
//    0        4     x
class HexElement : public Element {
 public:
  HexElement() : Element(8) {}
  unsigned NumEdges() const override { return 12; }
  void Edge(unsigned ei, unsigned& v1, unsigned& v2) const override;
  unsigned NumFaces() const override { return 6; }
  std::vector<unsigned> Face(unsigned fi) const override;

  ///@brief natural coordinate for a point in reference space
  Vec3f natCoord(const Vec3f& p, const std::vector<Vec3f>& X);
  ///@param p natural coordinates.
  ///@return interpolation weight for each dof.
  virtual std::vector<float> shapeFun(const Vec3f& p) const;

  ///@param ii index of basis function.
  ///@param xx point in natural coordinates.
  ///@param X global array of rest positions.
  virtual Vec3f shapeFunGrad(int ii, const Vec3f& xx) const;

  Matrix3f DefGradGauss2(int qi, const std::vector<Vec3f>& X,
                         const std::vector<Vec3f>& x) const override;
  Vec3f ShapeFunGradGauss2(unsigned qi, unsigned vi) const override;
  void InitJacobian(QUADRATURE_TYPE qtype,
                    const std::vector<Vec3f>& X) override;
};

// z
//    3
//         y
//       2
//    0       1   x
class TetElement : public Element {
 public:
  TetElement() : Element(4) {}
  unsigned NumEdges() const override { return 6; }
  void Edge(unsigned ei, unsigned& v1, unsigned& v2) const override;
  unsigned NumFaces() const override { return 4; }
  std::vector<unsigned> Face(unsigned fi) const override;
  /// UNIMPLEMENTED
  ///@param p natural coordinates.
  ///@return interpolation weight for each dof.
  virtual std::vector<float> shapeFun(const Vec3f& p) const;
  /// UNIMPLEMENTED
  ///@param ii index of basis function.
  ///@param xx point in natural coordinates.
  ///@param X global array of rest positions.
  virtual Vec3f shapeFunGrad(int ii, const Vec3f& xx) const;
};

#endif