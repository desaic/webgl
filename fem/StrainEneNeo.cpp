#include "StrainEneNeo.h"

StrainEneNeo::StrainEneNeo()
{
  param.resize(2);
  param[0] =1;
  param[1] =10;
}

double StrainEneNeo::getEnergy(const Matrix3f &F)
{
  double I1 = (F.transposed()*F).trace();
  double JJ = std::log(F.determinant());
  double mu = param[0],lambda=param[1];
  double Psi = (mu/2) * (I1-3) - mu*JJ + (lambda/2)*JJ*JJ;
  return Psi;
}

Matrix3f
StrainEneNeo::getPK1(const Matrix3f & F)
{
  double JJ = std::log(F.determinant());
  Matrix3f Finv = F.inverse().transposed();
  double mu = param[0],lambda=param[1];
  Matrix3f PP = mu*(F-Finv) + lambda*JJ*Finv;
  return PP;
}

std::vector<Matrix3f> StrainEneNeo::getdPdx(const Matrix3f& F, const Vec3f& dF,
                                            int dim) {
  Vec3f FinvTdF = _FinvT * dF;
  std::vector<Matrix3f> dP(dim, Matrix3f::Zero());
  for (int ii = 0; ii < dim; ii++) {
    dP[ii].setRow(ii, float(param[0]) * dF);
    dP[ii] += _c1 * OuterProd(FinvTdF, _FinvT.getRow(ii)) +
              float(param[1]) * FinvTdF[ii] * _FinvT;
  }
  return dP;
}

void StrainEneNeo::CacheF(const Matrix3f& F) {
  double JJ = std::log(F.determinant());
  double mu = param[0];
  double lambda = param[1];
  _c1 = mu - lambda * JJ;
  _FinvT = F.inverse();
  _FinvT.transpose();
}
