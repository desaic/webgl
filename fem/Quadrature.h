#ifndef QUADRATURE_H
#define QUADRATURE_H

#include <vector>
#include "Vec3.h"

enum class QUADRATURE_TYPE {NONE, GAUSS2};

class Quadrature
{
public:
  std::vector<Vec3f> x;
  std::vector<float> w;
  Quadrature();
  
  static const Quadrature Gauss2_2D;

  static const Quadrature Gauss2;
};
Quadrature makeGauss2();
#endif
