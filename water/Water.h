#ifndef WATER_H
#define WATER_H
#include "Array3D.h"
#include "Vec3.h"
class Water {
 public:
  int Initialize();
  int InitializeSimple();
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

  const Array3D<Vec3f>& U() { return u; }
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
  float density=1.0f; // reasonable value?
  int nIterations=50; // reasonable value?
  float overrelaxation=1.9f;
  float gravity = -9.81f;
  
  int boundary(unsigned x, unsigned y, unsigned z);
  float AvgU_y(unsigned i, unsigned j, unsigned k);
  float AvgU_z(unsigned i, unsigned j, unsigned k);
  float AvgV_x(unsigned i, unsigned j, unsigned k);
  float AvgV_z(unsigned i, unsigned j, unsigned k);  
  float AvgW_x(unsigned i, unsigned j, unsigned k);
  float AvgW_y(unsigned i, unsigned j, unsigned k);
};
#endif