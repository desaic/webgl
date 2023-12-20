#ifndef WATER_H
#define WATER_H
#include "Array3D.h"
#include "Vec3.h"

enum class Axis {
  X = 0,
  Y,
  Z
};

class Water {
 public:
  int Step();
  int AdvectU();
  int AddBodyForce();
  int SolveP();
  int AdvectPhi();
  int AdvectSmoke();

  int Allocate(unsigned sx, unsigned sy, unsigned sz);
  void SetBoundary();
  void SetInitialSmoke();
  void SetInitialVelocity();
  //linear 
  float InterpU(Vec3f& x, Axis axis);
  //trilinear
  float InterpPhi(const Vec3f& x);
  float InterpSmoke(Vec3f& x);


  const Array3D<Vec3f>& U() const { return u; }
  Array3D<Vec3f>& U() { return u; }
  const Array3D<float>& P() { return p; }

  const Array3D<float>& Smoke() const { return smoke; }
  Array3D<float>& Smoke() { return smoke; }  
 private:
  // Boundary
  Array3D<uint8_t> s;

  //defined on face centers
  // size is +1 of voxel grid size.
  Array3D<Vec3f> u;
  Array3D<Vec3f> uTmp;

  // smoke
  Array3D<float> smoke;
  Array3D<float> smokeTemp;

  //defined on vertices
  //size is +1 of voxel grid size.
  Array3D<float> phi;
  //intermediate variable pressure.
  Array3D<float> p;
  float dt = 0.01666667f;
  float h = 0.02f;
  float density = 1000.f; // reasonable value?
  int nIterations = 200; // reasonable value?
  float overrelaxation = 1.9f;
  float gravity = 0.f; //-9.81f;
  Vec3u numCells;
  
  float AvgU_y(unsigned i, unsigned j, unsigned k);
  float AvgU_z(unsigned i, unsigned j, unsigned k);
  float AvgV_x(unsigned i, unsigned j, unsigned k);
  float AvgV_z(unsigned i, unsigned j, unsigned k);  
  float AvgW_x(unsigned i, unsigned j, unsigned k);
  float AvgW_y(unsigned i, unsigned j, unsigned k);
};
#endif