#ifndef WATER_H
#define WATER_H
#include "Vec3.h"
#include "Array3D.h"
class Water{
  public:
    int Step(){return 0;}
  private:
    Array3D<Vec3f> u;
	Array3D<float> phi;	
};
#endif