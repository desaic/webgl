#include "StableNeo.h"

StableNeo::StableNeo() {
  param.resize(3);
  param[0] =1e6f;
  param[1] =1e7f;
  param[2] = 1.1;
}

void StableNeo::SetParam(const std::vector<float>& in) {
  //Lame reparameterization
  float lambda = in[1];
  float alpha = 1.0 + in[0] / (in[1] + 1e-9f);
  param.resize(3);
  param[0] = in[0];
  param[1] = lambda;
  param[2] = alpha;
}

double StableNeo::getEnergy(const Matrix3f& F) {
  double Ic = F.squaredNorm();
  double JminusA = F.determinant() - Alpha();
  double Psi = 0.5 * (Mu() * (Ic - 3.0) + Lambda() * JminusA * JminusA);
  return Psi;
}

Matrix3f partialJpartialF(const Matrix3f& F) {
  Matrix3f pJpF;
  pJpF.setCol(0, F.getCol(1).cross(F.getCol(2)));
  pJpF.setCol(1, F.getCol(2).cross(F.getCol(0)));
  pJpF.setCol(2, F.getCol(0).cross(F.getCol(1)));
  return pJpF;
}

Matrix3f StableNeo::getPK1(const Matrix3f& F) {
  Matrix3f pJpF = partialJpartialF(F);
  const double Jminus1 = F.determinant() - Alpha();
  Matrix3f PP = Mu() * F + Lambda() * Jminus1 * pJpF;
  return PP;
}

//matrix form of cross product with ith column
Matrix3f colCrossProductMat(const Matrix3f& F, const int i) {
  Matrix3f c;
  c(0, 1) = -F(2, i);
  c(0, 2) = F(1, i);
  c(1, 0) = F(2, i);
  c(1, 2) = -F(0, i);
  c(2, 0) = -F(1, i);
  c(2, 1) = F(0, i);
  return c;
}

std::vector<Matrix3f> StableNeo::getdPdx(const Matrix3f& F, const Vec3f& dF,
                                            int dim) {
  std::vector<Matrix3f> dP(dim, Matrix3f::Zero());
  double I3 = F.determinant();
  double scale = Lambda() * (I3 - Alpha()); 

  const Matrix3f f0hat = scale * colCrossProductMat(F, 0);
  const Matrix3f f1hat = scale * colCrossProductMat(F, 1);
  const Matrix3f f2hat = scale * colCrossProductMat(F, 2);
  for (int ii = 0; ii < dim; ii++) {
    //mu * I * dFi
    dP[ii].setRow(ii, float(Mu()) * dF);
    //lambda * pJpF * pJpF^T * dF
    dP[ii] += (Lambda() * _pJpF.getRow(ii).dot(dF)) * _pJpF;
    Matrix3f hessJ;
    Vec3f colVec = -dF[1] * f2hat.getCol(ii) + dF[2] * f1hat.getCol(ii);    
    hessJ.setCol(0, colVec);
    colVec = dF[0] * f2hat.getCol(ii) - dF[2] * f0hat.getCol(ii);
    hessJ.setCol(1, colVec);
    colVec = -dF[0] * f1hat.getCol(ii) + dF[1] * f0hat.getCol(ii);    
    hessJ.setCol(2, colVec);
    dP[ii] += hessJ;
  }
  return dP;
}

void StableNeo::CacheF(const Matrix3f& F) { _pJpF = partialJpartialF(F); }
