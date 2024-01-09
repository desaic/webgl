#ifndef WATER_H
#define WATER_H
#include "Array3D.h"
#include "Vec3.h"
#include "b3Clock.h"

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
  int UpdateSDF();
  int MarchSmoke();

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

  const Array3D<uint8_t>& SmokeBoundary() const { return smokeBoundary; }
  Array3D<uint8_t>& SmokeBoundary() { return smokeBoundary; }  

  const Array3D<short>& SmokeSDF() const { return smokeSDF; }
  Array3D<short>& SmokeSDF() { return smokeSDF; }  
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
  Array3D<uint8_t> smokeBoundary;
  Array3D<short> smokeSDF;
  const float SDFLevel = 0.1;
  const float SDFUnit = 0.001;
  const unsigned SDFBand = 5;

  //defined on vertices
  //size is +1 of voxel grid size.
  Array3D<float> phi;
  //intermediate variable pressure.
  Array3D<float> p;
  float dt = 0.01666667f;
  float h = 0.02f;
  float density = 1000.f; // reasonable value?
  int nIterations = 100; // reasonable value?
  float overrelaxation = 1.9f;
  float gravity = 0.f; //-9.81f;
  Vec3u numCells;
  size_t steps = 0;
  b3Clock clock;
  
  float AvgU_y(unsigned i, unsigned j, unsigned k);
  float AvgU_z(unsigned i, unsigned j, unsigned k);
  float AvgV_x(unsigned i, unsigned j, unsigned k);
  float AvgV_z(unsigned i, unsigned j, unsigned k);  
  float AvgW_x(unsigned i, unsigned j, unsigned k);
  float AvgW_y(unsigned i, unsigned j, unsigned k);
};
#endif