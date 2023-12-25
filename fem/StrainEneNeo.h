#ifndef STRAINENENEO_H
#define STRAINENENEO_H
#include "StrainEne.h"

class StrainEneNeo:public StrainEne
{
public:
  StrainEneNeo();
  double getEnergy(const Matrix3f & F);
  Matrix3f getPK1(const Matrix3f & F);
  std::vector<Matrix3f>
  getdPdx(const Matrix3f &F, const Vec3f &dF, int dim=3);

  Matrix3f cacheFinvT;
  double c1;
};
#endif
