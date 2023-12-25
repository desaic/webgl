#ifndef STRAIN_LIN_HPP
#define STRAIN_LIN_HPP
#include "StrainEne.h"

class StrainLin :public StrainEne{
public:
	StrainLin();
  virtual double getEnergy(const Matrix3f & F);
  virtual Matrix3f getPK1(const Matrix3f & F);
  virtual std::vector<Matrix3f> getdPdx(const Matrix3f & F,
                                  const Vec3f & dF, int dim=3);
};

#endif
