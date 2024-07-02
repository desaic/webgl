#ifndef STRAINENE_H
#define STRAINENE_H

#include <iostream>
#include <vector>

#include "Matrix3f.h"
#include "Vec3.h"

///@brief abstract class for strain energy functions, i.e. function of
///deformation gradient also computes derivatives
class StrainEne {
 public:
  StrainEne() {}
  virtual double getEnergy(const Matrix3f& F) = 0;
  virtual Matrix3f getPK1(const Matrix3f& F) = 0;
  virtual std::vector<Matrix3f> getdPdx(const Matrix3f& F, const Vec3f& dF,
                                        int dim = 3) = 0;
  virtual void CacheF(const Matrix3f& F) {}
  virtual ~StrainEne();
  virtual void SetParam(const std::vector<float>& in) { param = in; }
  std::vector<float> param;

  enum MaterialModel { LIN, COROT, NEO, SNH };
};

std::vector<StrainEne*> loadMaterials(std::istream& in,
                                      std::vector<double>& densities);

///@param E array of 2 doubles. E and nu
///@param mu array of size 2. mu and lambda.
void Young2Lame(double E, double nu, float* mu);
std::vector<StrainEne*> loadMaterials(std::istream& in,
                                      std::vector<double>& densities);
#endif
