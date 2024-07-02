#ifndef STABLENEO_H
#define STABLENEO_H
#include "StrainEne.h"
///parameterized by mu, lambda and alpha in param
class StableNeo : public StrainEne {
 public:
  StableNeo();
  double getEnergy(const Matrix3f& F);
  Matrix3f getPK1(const Matrix3f& F);
  std::vector<Matrix3f> getdPdx(const Matrix3f& F, const Vec3f& dF,
                                int dim = 3);
  void CacheF(const Matrix3f& F) override;
  void SetParam(const std::vector<float>&param)override;
  float Mu() const { return param[0]; }
  float Lambda() const { return param[1]; }
  float Alpha() const { return param[2]; }

  Matrix3f _pJpF;
  
};
#endif
