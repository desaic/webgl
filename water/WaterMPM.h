#pragma once

#include "Array3D.h"
#include "Vec3.h"
#include "b3Clock.h"

enum class Axis {
  X = 0,
  Y,
  Z
};

class Particle {
 public:
  Particle() {}
  Particle(Vec3f& pos): position(pos), velocity(Vec3f(0)) {}
  Particle(Vec3f& pos, Vec3f& vel): position(pos), velocity(vel) {}

  Vec3f position;
  Vec3f velocity;
};

class WaterMPM {
 public:
  int Step();

  int Allocate(unsigned sx, unsigned sy, unsigned sz);
  
  int ParticlesToGrid();
  int AdvanceGrid();
  int GridToParticles();
  int AdvanceParticles();

  //linear 
  float InterpU(Vec3f& x, Axis axis);

  const Array3D<Vec3f>& VelocityGrid() const { return velocityGrid; }
  Array3D<Vec3f>& VelocityGrid() { return velocityGrid; }

  const Array3D<float>& MassGrid() const { return massGrid; }
  Array3D<float>& MassGrid() { return massGrid; }

  const std::vector<Particle>& Particles() const { return particles; }
  std::vector<Particle>& Particles() { return particles; }

 private:
  // Boundary
  Array3D<uint8_t> s;

  // Particles
  std::vector<Particle> particles;

  // Grids
  Array3D<Vec3f> velocityGrid;
  Array3D<float> massGrid;

  //defined on vertices
  //size is +1 of voxel grid size.
  Array3D<float> phi;
  //intermediate variable pressure.
  Array3D<float> p;
  float dt = 0.01666667f;
  //meters? 2cm
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