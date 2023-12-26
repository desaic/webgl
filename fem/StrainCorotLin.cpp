#include "StrainCorotLin.h"
#include "Matrix3fSVD.h"
#include <vector>

std::vector<Matrix3f> initEp3();
//3D Levi-Civita symbol
std::vector<Matrix3f> Ep3=initEp3();

std::vector<Matrix3f> initEp3()
{
  std::vector<Matrix3f> Ep3(3, Matrix3f::Zero());
  Ep3[0](1, 2) = 1;
  Ep3[0](2, 1) = -1;

  Ep3[1](0, 2) = -1;
  Ep3[1](2, 0) = 1;

  Ep3[2](0, 1) = 1;
  Ep3[2](1, 0) = -1;

  return Ep3;
}

double dot(Matrix3f m1, Matrix3f m2){
  double prod = 0;
  for (int ii = 0; ii < 3; ii++){
    for (int jj = 0; jj < 3; jj++){
      prod += m1(ii, jj) * m2(ii, jj);
    }
  }
  return prod;
}

StrainCorotLin::StrainCorotLin()
{
  param.resize(2);
  param[0] = 1;
  param[1] = 10;
}

double StrainCorotLin::getEnergy(const Matrix3f &F)
{
  Matrix3f U, V;
  Vec3f Sigma = SVD(F, U, V) - Vec3f(1, 1, 1);
  double mu = param[0];
  double lambda = param[1];
  Matrix3f I = Matrix3f::identity();
  double t = Sigma[0] + Sigma[1] + Sigma[2];
  double Psi = mu*(Sigma[0] * Sigma[0] + Sigma[1] * Sigma[1] + Sigma[2] * Sigma[2]) + 0.5f * lambda * t * t;
  return Psi;
}

Matrix3f StrainCorotLin::getPK1(const Matrix3f & F)
{
  Matrix3f U, V;
  Vec3f Sigma = SVD(F, U, V);
  Matrix3f R = U * V.transposed();
  double mu = param[0];
  double lambda = param[1];
  Matrix3f I = Matrix3f::identity();
  Matrix3f P = 2*mu*(F-R) + lambda * (R.transposed() * F - I).trace() * R;

  return P;
}

Matrix3f crossProdMat(const Vec3f & v)
{
  Matrix3f A = Matrix3f::Zero();
  A(0, 1) = -v[2];
  A(0, 2) = v[1];
  A(1, 0) = v[2];
  A(1, 2) = -v[0];
  A(2, 0) = -v[1];
  A(2, 1) = v[0];
  return A;
}

std::vector<Matrix3f> StrainCorotLin::getdPdx(const Matrix3f& F,
                                              const Vec3f& _dF, int dim) {
  Matrix3f U, V;
  Vec3f sVec = SVD(F, U, V);
  Matrix3f R = U * V.transposed();
  Matrix3f Sigma = Matrix3f::Diag(sVec[0],sVec[1],sVec[2]);
  Matrix3f S = V*Sigma*V.transposed();
  Matrix3f I = Matrix3f::identity();
  Matrix3f SI = (S.trace()*I - S).inverse();

  //debug dR computation
  //double h = 0.01;
  //Eigen::Matrix3d F1 = F + h*dF;
  //svd.compute(F1, Eigen::ComputeFullU | Eigen::ComputeFullV);
  //U = svd.matrixU();
  //V = svd.matrixV();
  //Eigen::Matrix3d R1 = U * V.transpose();
  //R1 -= R;
  //R1 = (1/h)*R1;
  //std::cout << dR << "\n" << R1 << "\n\n";

  double mu = param[0];
  double lambda = param[1];

  std::vector<Matrix3f> dP(dim);

  for(int ii =0 ; ii<dim; ii++){
    Matrix3f dF=Matrix3f::Zero();
    dF.setRow(ii, _dF);
    Matrix3f W = R.transposed()*dF;
    Vec3f w;
    w[0] = W(1, 2) - W(2, 1);
    w[1] = W(2, 0) - W(0, 2);
    w[2] = W(0, 1) - W(1, 0);
    Matrix3f dR = -R*crossProdMat(SI*w);
    dP[ii] = 2 * mu*dF + lambda*W.trace()*R +(lambda*(S - I).trace() - 2 * mu)*dR;
  }
  return dP;
}
