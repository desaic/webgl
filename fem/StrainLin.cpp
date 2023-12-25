#include "StrainLin.h"

StrainLin::StrainLin()
{
	param.resize(2);
	param[0] = 1;
	param[1] = 10;
}

double StrainLin::getEnergy(const Matrix3f & F)
{
  Matrix3f I = Matrix3f::identity();
  Matrix3f eps = 0.5f*(F + F.transposed()) - I;
  double t = eps.trace();
  double Psi = param[0]*eps.squaredNorm() + 0.5f*param[1] * t*t;
	return Psi;
}

Matrix3f
StrainLin::getPK1(const Matrix3f & F)
{
  double mu = param[0];
  double lambda = param[1];
  Matrix3f I = Matrix3f::identity();
  Matrix3f PP = mu*(F + F.transposed()-2*I) + lambda * (F.trace()-3) * I;
	return PP;
}

std::vector<Matrix3f>
StrainLin::getdPdx(const Matrix3f &F,
                   const Vec3f &_dF, int dim)
{
  double mu = param[0];
  double lambda = param[1];
  Matrix3f I = Matrix3f::identity();
  std::vector<Matrix3f> dP(dim);
  for(int ii =0 ; ii<dim; ii++){
    Matrix3f dF = Matrix3f::Zero();
    dF.setRow(ii, _dF);
    dP[ii] = mu * (dF + dF.transposed()) + lambda * dF(ii,ii) * I;
  }

	return dP;
}
