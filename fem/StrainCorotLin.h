#ifndef STRAINCOROTLIN_H
#define STRAINCOROTLIN_H
#include "StrainLin.h"

class StrainCorotLin :public StrainLin{
public:
  StrainCorotLin();
  virtual double getEnergy(const Matrix3f & F);
  virtual Matrix3f getPK1(const Matrix3f & F);
  virtual std::vector<Matrix3f>
    getdPdx(const Matrix3f & F, const Vec3f & dF, int dim=3);

};

#endif
