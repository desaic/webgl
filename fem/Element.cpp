#include "Element.h"

Vec3f Element::GetDisp(const Vec3f& p, const std::vector<Vec3f>& X,
                      const std::vector<Vec3f>& x) {
  std::vector<float> w = shapeFun(p);
  Vec3f u(0, 0, 0);
  for (unsigned int ii = 0; ii < w.size(); ii++) {
    u += w[ii] * (x[ii] - X[ii]);
  }
  return u;
}

//z     3        7
//   1        5
//        y
//      2        6
//   0        4     x
unsigned HexEdges[12][2] = {{0, 4}, {2, 6}, {1, 5}, {3, 7}, {0, 2}, {4, 6},
                            {1, 3}, {5, 7}, {0, 1}, {2, 3}, {4, 5}, {6, 7}};

/// cached shape function gradient for the combination of hex element and 
/// 2-point gaussian quadrature.
Vec3f HexGrauss2Grad[8][8];

void HexElement::Edge(unsigned ei, unsigned& v1, unsigned& v2) const {
  if (ei < 12) {
    v1 = _v[HexEdges[ei][0]];
    v2 = _v[HexEdges[ei][1]];
  }
}
unsigned HexFaces[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1},
                           {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
std::vector<unsigned> HexElement::Face(unsigned fi) const {
  std::vector<unsigned> f;
  if (fi < 6) {
    f.resize(4);
    for (unsigned i = 0; i < 4; i++) {
      f[i] = _v[HexFaces[fi][i]];
    }
  }
  return f;
}

Vec3f HexElement::natCoord(const Vec3f& p, const std::vector<Vec3f>& X) {
  Vec3f n = p - X[_v[0]];
  double size = (X[_v[7]][0] - X[_v[0]][0]);
  n = float(2.0f / size) * n - Vec3f(1, 1, 1);
  return n;
}

const float HexW[8][3] = {{-1, -1, -1}, {-1, -1, 1}, {-1, 1, -1}, {-1, 1, 1},
                          {1, -1, -1},  {1, -1, 1},  {1, 1, -1},  {1, 1, 1}};

///@param p natural coordinates.
///@return interpolation weight for each dof.
std::vector<float> HexElement::shapeFun(const Vec3f& p) const {
  std::vector<float> weights(NumVerts());
  for (int ii = 0; ii < NumVerts(); ii++) {
    weights[ii] = (1.0f / 8) * (1 + HexW[ii][0] * p[0]) *
                  (1 + HexW[ii][1] * p[1]) * (1 + HexW[ii][2] * p[2]);
  }
  return weights;
}

///@param ii index of basis function.
///@param xx point in natural coordinates.
///@param X global array of rest positions.
Vec3f HexElement::shapeFunGrad(int ii, const Vec3f& xx) const {
  Vec3f grad;
  grad[0] = 0.125 * HexW[ii][0] * (1 + HexW[ii][1] * xx[1]) *
            (1 + HexW[ii][2] * xx[2]);
  grad[1] = 0.125 * HexW[ii][1] * (1 + HexW[ii][0] * xx[0]) *
            (1 + HexW[ii][2] * xx[2]);
  grad[2] = 0.125 * HexW[ii][2] * (1 + HexW[ii][0] * xx[0]) *
            (1 + HexW[ii][1] * xx[1]);
  return grad;
}

Matrix3f HexElement::DefGradGauss2(int qi, const std::vector<Vec3f>& X,
                       const std::vector<Vec3f>& x) const {
  Matrix3f F = Matrix3f::Zero();
  for (int ii = 0; ii < NumVerts(); ii++) {
    F += OuterProd(x[_v[ii]], HexGrauss2Grad[qi][ii]);
  }
  return F * _Jinv[qi];
}

void HexElement::InitJacobian(QUADRATURE_TYPE qtype,
                              const std::vector<Vec3f>& X) {
  if (qtype == QUADRATURE_TYPE::GAUSS2) {
    const auto& q = Quadrature::Gauss2;
    _Jinv.resize(q.x.size());
    _detJ.resize(q.x.size());
    for (int qq = 0; qq < q.x.size(); qq++) {
      Matrix3f J = Matrix3f::Zero();
      for (int ii = 0; ii < NumVerts(); ii++) {
        J += OuterProd(X[_v[ii]], HexGrauss2Grad[qq][ii]);
      }
      _Jinv[qq] = J.inverse();
      _detJ[qq] = J.determinant();
    }
  }
}

// z    
//    3 
//         y
//       2   
//    0       1   x
unsigned TetEdges[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {2, 3}, {3, 1}};
void TetElement::Edge(unsigned ei, unsigned& v1, unsigned& v2) const {
  if (ei < 6) {
    v1 = _v[TetEdges[ei][0]];
    v2 = _v[TetEdges[ei][1]];
  }
}

unsigned TetFaces[4][3] = {{0, 1, 3}, {0, 2, 1}, {0, 3, 2}, {1, 2, 3}};
std::vector<unsigned> TetElement::Face(unsigned fi) const {
  std::vector<unsigned> f;
  if (fi < 4) {
    f.resize(3);
    for (unsigned i = 0; i < 3; i++) {
      f[i] = _v[TetFaces[fi][i]];
    }
  }
  return f;
}

///@param p natural coordinates.
///@return interpolation weight for each dof.
std::vector<float> TetElement::shapeFun(const Vec3f& p) const {
  return std::vector<float>();
}

///@param ii index of basis function.
///@param xx point in natural coordinates.
///@param X global array of rest positions.
Vec3f TetElement::shapeFunGrad(int ii, const Vec3f& xx) const {
  return Vec3f();
}
