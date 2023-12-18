#pragma once

#include "Array2D.h"

#include <vector>

class ShallowWater {
 public:
  void Init(unsigned sizex, unsigned sizey);
  /// only uses the first row
  void Step1D(float dt);

  const Array2Df& GetH() const { return _H; }
  
  const Array2Df& Geth() const { return _h; }
  // m/s
  const Array2Df& Getu() const { return _u; }
  const Array2Df& Getv() const { return _v; }

  // does not resize h.
  void SethRow(const std::vector<float>& vals, unsigned row);

 private:
   /// TODO add padding.
  /// depth of ground relative to 0. going downwards is positive.
   //m
  Array2Df _H;
  
  /// water height relative to 0. not relative to ground
  /// staggered grid. last row and col are unused. 
  /// values are shifted by dx/2 in space.
  Array2Df _h;
  //m/s
  Array2Df _u, _v;

  //m/s2
  float g = 9.8;
  //m
  float _dx = 0.01f;
};
