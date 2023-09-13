#ifndef WATER_H
#define WATER_H
#include "Array3D.h"
#include "Vec3.h"
class Water {
 public:
  int Step();
  int AdvectU();
  int AddBodyForce();
  int SolveP();
  int AdvectPhi();

  int Allocate(unsigned sx, unsigned sy, unsigned sz);
  //linear 
  Vec3f InterpU(const Vec3f& x);
  //trilinear
  float InterpPhi(const Vec3f& x);
 private:
  //defined on face centers
  // size is +1 of voxel grid size.
  Array3D<Vec3f> u;
  Array3D<Vec3f> uTmp;
  //defined on vertices
  //size is +1 of voxel grid size.
  Array3D<float> phi;
  //intermediate variable pressure.
  Array3D<float> p;
  float dt=0.01f;
  float h=0.1f;
};
#endif