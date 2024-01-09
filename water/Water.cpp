#include "Water.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include "FileWriter.h"
#include "sdf/MarchingCubes.h"
#include "sdf/FastSweep.h"
#include "TrigMesh.h"

// @sx/@sy/@sz -> number of cells in each dimension
int Water::Allocate(unsigned sx, unsigned sy, unsigned sz) {
  const unsigned numX = sx + 2;
  const unsigned numY = sy + 2;
  const unsigned numZ = sz + 2;
  u.Allocate(numX, numY, numZ);
  uTmp.Allocate(numX, numY, numZ);
  smoke.Allocate(numX, numY, numZ);
  smokeTemp.Allocate(numX, numY, numZ);
  smokeBoundary.Allocate(numX, numY, numZ);
  smokeSDF.Allocate(numX, numY, numZ);
  phi.Allocate(numX+1, numY+1, numZ+1);  // Initialize phi?
  p.Allocate(numX, numY, numZ);
  numCells = Vec3u(numX, numY, numZ);
  SetBoundary();
  SetInitialSmoke();
  //SetInitialVelocity();

  return 0;
}

void Water::SetInitialVelocity() {
  for (int y = 2; y < numCells[1]-2; ++y) {
    u(2, y, 2)[0] = 2.0f;
  }
}

void Water::SetInitialSmoke() {
  for (int x = 1; x < numCells[0]-1; ++x) {
    for (int y = 1; y < numCells[1]-1; ++y) {
      for (int z = 1; z < numCells[2]-1; ++z) {
        smoke(x,y,z) = 1.0f;
      }
    }
  }

  const float smokeWidth = 0.25 * numCells[1];
  const int minY = floor(0.5 * numCells[1] - 0.5 * smokeWidth);
  const int maxY = floor(0.5 * numCells[1] + 0.5 * smokeWidth);

  for (int y = minY; y < maxY; ++y) {
    for (int z = 0; z < numCells[2]; ++z) {
      smoke(0, y, z) = 0.0f;
    }
  }
}

void Water::SetBoundary() {
  s.Allocate(numCells[0], numCells[1], numCells[2]);
  for (int i = 0; i < numCells[0]; ++i) {
    for (int j = 0; j < numCells[1]; ++j) {
      for (int k = 0; k < numCells[2]; ++k) {
        uint8_t val = 1;
        if (i == 0 || j == 0 || k == 0 || 
            i == numCells[0] - 1 ||
            j == numCells[1] - 1 ||
            k == numCells[2] - 1) 
          val = 0;
        s(i, j, k) = val;
      }
    }
  }
}

int Water::AdvectU() {
  const float h2 = h / 2.0;
  const Vec3u& sz = u.GetSize();
  uTmp = u;
  for (int i = 1; i < sz[0]; ++i) {
    for (int j = 1; j < sz[1]; ++j) {
      for (int k = 1; k < sz[2]; ++k) {
        if (s(i,j,k) == 0) continue;
        // x
        if (s(i-1, j, k) > 0 && j < sz[1] - 1 && k < sz[2] - 1) {
          float x = i*h;
          float y = j*h + h2;
          float z = k*h + h2;

          float uX = u(i, j, k)[0];
          float uY = AvgU_y(i, j, k);
          float uZ = AvgU_z(i, j, k);

          x -= dt * uX;
          y -= dt * uY;
          z -= dt * uZ;

          uTmp(i, j, k)[0] = InterpU(Vec3f(x, y, z), Axis::X);
        }
        // y
        if (s(i, j - 1, k) > 0 && i < sz[0] - 1 && k < sz[2] - 1) {
          float x = i*h + h2;
          float y = j*h;
          float z = k*h + h2;

          float uX = AvgV_x(i, j, k);
          float uY = u(i, j, k)[1];
          float uZ = AvgV_z(i, j, k);
  
          x -= dt * uX;
          y -= dt * uY;
          z -= dt * uZ;

          uTmp(i, j, k)[1] = InterpU(Vec3f(x, y, z), Axis::Y);
        }
        // z
        if (s(i, j, k - 1) > 0 && i < sz[0] - 1 && j < sz[1] - 1) {
          float x = i*h + h2;
          float y = j*h + h2;
          float z = k*h;

          float uX = AvgW_x(i, j, k);
          float uY = AvgW_y(i, j, k);
          float uZ = u(i, j, k)[2];
          
          x -= dt * uX;
          y -= dt * uY;
          z -= dt * uZ;

          uTmp(i, j, k)[2] = InterpU(Vec3f(x, y, z), Axis::Z);
        }
      }
    }
  } 

  u = uTmp;
  return 0;
}

int Water::AdvectPhi() { 
  const Vec3u& sz = u.GetSize();
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) { 

      }
    }
  }
  return 0;
}

int Water::AdvectSmoke() {
  const float halfH = h / 2.0f;
  smokeTemp = smoke;

  for (int i = 1; i < numCells[0] - 1; ++i) {
    for (int j = 1; j < numCells[1] - 1; ++j) {
      for (int k = 1; k < numCells[2] - 1; ++k) {
        if (s(i, j, k) == 0) continue;
        const float uX = 0.5 * (u(i, j, k)[0] + u(i+1, j, k)[0]);
        const float uY = 0.5 * (u(i, j, k)[1] + u(i, j+1, k)[1]);
        const float uZ = 0.5 * (u(i, j, k)[2] + u(i+1, j, k+1)[2]);

        const float x = halfH + (h*i) - (dt*uX);
        const float y = halfH + (h*j) - (dt*uY);
        const float z = halfH + (h*k) - (dt*uZ);

        smokeTemp(i, j, k) = InterpSmoke(Vec3f(x,y,z));
      }
    }
  }
  smoke = smokeTemp;
  return 0;
}

int Water::AddBodyForce() {
    const Vec3u& sz = u.GetSize();
    for (int i = 1; i < sz[0] - 1; ++i) {
      for (int j = 1; j < sz[1] - 1; ++j) {
        for (int k = 1; k < sz[2] - 1; ++k) {
          if (s(i, j, k) == 1 && s(i, j-1, k) == 1)
            u(i, j, k)[1] += gravity * dt;
        }  
      }
    }

    for (int x = 1; x < sz[0] - 1; ++x) {
      for (int z = 1; z < sz[2] - 1; ++z) {
        u(x, sz[1] - 2, z)[0] += 1*dt;
      }
    }

    return 0;
}

// adapted from https://github.com/matthias-research/pages/blob/master/tenMinutePhysics/17-fluidSim.html
int Water::SolveP() { 
    if (numCells[0] == 0 || 
        numCells[1] == 0 || 
        numCells[2] == 0) return -1; // empty grid
        
    float cp = density * h / dt;

    for (int iter = 0; iter < nIterations; ++iter) {
        for (int x = 1; x < numCells[0] - 1; ++x) {
            for (int y = 1; y < numCells[1] - 1; ++y) {
                for (int z = 1; z < numCells[2] - 1; ++z) {
                    if (s(x,y,z) == 0) continue;

                    int sx0 = s(x-1, y  , z  );
                    int sx1 = s(x+1, y  , z  );
                    int sy0 = s(x  , y-1, z  );
                    int sy1 = s(x  , y+1, z  );
                    int sz0 = s(x  , y  , z-1);
                    int sz1 = s(x  , y  , z+1);

                    int s = sx0 + sx1 + sy0 + sy1 + sz0 + sz1; 

                    if (s == 0) continue;
                    float divergence = (
                        u(x+1, y  , z  )[0] - u(x, y, z)[0] +
                        u(x  , y+1, z  )[1] - u(x, y, z)[1] +
                        u(x  , y  , z+1)[2] - u(x, y, z)[2]
                    );

                    float pressure =  -divergence / float(s);
                    pressure *= overrelaxation;
                        
                    p(x, y, z) += cp * pressure;

                    // Update velocity field
                    u(x  , y  , z  )[0] -= pressure * sx0;
                    u(x+1, y  , z  )[0] += pressure * sx1;
                    u(x  , y  , z  )[1] -= pressure * sy0;
                    u(x  , y+1, z  )[1] += pressure * sy1;                        
                    u(x  , y  , z  )[2] -= pressure * sz0;
                    u(x  , y  , z+1)[2] += pressure * sz1;
                }
            }
        }
    }

    return 0;
}

int Water::UpdateSDF() {
  const float smokeThreshold = 0.5f; // smoke voxels are below the threshold

  // voxel is on the smoke boundary if it's not smoke itself but has a smoke neighbor
  for (int x = 1; x < numCells[0] - 1; ++x) {
    for (int y = 1; y < numCells[1] - 1; ++y) {
      for (int z = 1; z < numCells[2] - 1; ++z) {
        if (smoke(x, y, z) > smokeThreshold) { 
          smokeBoundary(x,y,z) = 0; 
        } else {
          uint8_t isBoundary = 0;
          for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
              for (int dz = -1; dz <= 1; dz++) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                if (smoke(x+dx, y+dx, z+dz) > smokeThreshold) {
                  isBoundary = 1;     
                }
              } 
            } 
          }
          smokeBoundary(x,y,z) = isBoundary;
        }
      }
    }  
  }

  const float MaxDistanceMM = h * (SDFBand + 1);

  smokeSDF.Fill(round(MaxDistanceMM/SDFUnit)); // far distance?

  for (int x = 1; x < numCells[0] - 1; ++x) {
    for (int y = 1; y < numCells[1] - 1; ++y) {
      for (int z = 1; z < numCells[2] - 1; ++z) {
        float smokeVal = smoke(x,y,z);
        if (smokeVal < smokeThreshold) {
          smokeSDF(x,y,z) = 0;
        } else {
          smokeSDF(x,y,z) = smokeVal / SDFUnit;
        }
      }
    }
  }

  FastSweep(smokeSDF, h, SDFUnit, SDFBand, smokeBoundary);

  return 0;
}

int Water::MarchSmoke() {
  TrigMesh mesh;
  for (int x = 0; x < numCells[0] - 1; ++x) {
    for (int y = 0; y < numCells[1] - 1; ++y) {
      for (int z = 0; z < numCells[2] - 1; ++z) {
        MarchOneCube<short>(x, y, z, smokeSDF, SDFLevel/SDFUnit, &mesh, h/SDFUnit);
      }
    }
  }

  steps++;
  if (steps == 50 || steps == 100 || steps == 200) {
    std::string filename = "C:\\Data\\stls\\debug_water" + std::to_string(steps) + ".obj";
    mesh.SaveObj(filename);
  }
        
  return 0;
}

int Water::Step() {
  clock.reset();
  AddBodyForce();
  unsigned long bodyForceTime = clock.getTimeMilliseconds();
  clock.reset();

  SolveP();
  unsigned long solvePTime = clock.getTimeMilliseconds();
  clock.reset();

  AdvectU();
  unsigned long advectUTime = clock.getTimeMilliseconds();
  clock.reset();

  AdvectSmoke();
  unsigned long advectSmokeTime = clock.getTimeMilliseconds();
  clock.reset();

  UpdateSDF();
  unsigned long updateSDFTime = clock.getTimeMilliseconds();
  clock.reset();

  MarchSmoke();
  unsigned long marchingCubesTime = clock.getTimeMilliseconds();
  clock.reset();

  //std::cout << "Took " << bodyForceTime << ", " << solvePTime << ", " << advectUTime << ", " << advectSmokeTime << ", " << updateSDFTime <<
  //  ", " << marchingCubesTime << " milliseconds" << std::endl;
  return 0;
}

float Water::AvgU_y(unsigned i, unsigned j, unsigned k) {
  return 0.25f * ( 
    u(i,   j,   k)[1] +
    u(i-1, j,   k)[1] +
    u(i,   j+1, k)[1] +
    u(i-1, j+1, k)[1]
  );
}

float Water::AvgU_z(unsigned i, unsigned j, unsigned k) {
  return 0.25f * ( 
    u(i,   j,   k  )[2] +
    u(i-1, j,   k  )[2] +
    u(i,   j,   k+1)[2] +
    u(i-1, j,   k+1)[2]
  );
}

float Water::AvgV_x(unsigned i, unsigned j, unsigned k) {
  return 0.25f * ( 
    u(i,   j,   k)[0] +
    u(i+1, j,   k)[0] +
    u(i,   j-1, k)[0] +
    u(i+1, j-1, k)[0]
  );
}

float Water::AvgV_z(unsigned i, unsigned j, unsigned k) {
  return 0.25f * ( 
    u(i,   j,   k  )[2] +
    u(i,   j-1, k  )[2] +
    u(i,   j,   k+1)[2] +
    u(i,   j-1, k+1)[2]
  );
}

float Water::AvgW_x(unsigned i, unsigned j, unsigned k) {
  return 0.25f * ( 
    u(i,   j,   k  )[0] +
    u(i+1, j,   k  )[0] +
    u(i,   j,   k-1)[0] +
    u(i+1, j,   k-1)[0]
  );
}

float Water::AvgW_y(unsigned i, unsigned j, unsigned k) {
  return 0.25f * ( 
    u(i,   j,   k  )[1] +
    u(i,   j+1, k  )[1] +
    u(i,   j,   k-1)[1] +
    u(i,   j+1, k-1)[1]
  );
}

float Water::InterpU(Vec3f& x, Axis axis) { 
  const Vec3u& sz = u.GetSize();

  const float halfH = h / 2.0f;

  x[0] = std::max(std::min(x[0], sz[0] * h), h);
  x[1] = std::max(std::min(x[1], sz[1] * h), h);
  x[2] = std::max(std::min(x[2], sz[2] * h), h);

  float dx = 0.f;
  float dy = 0.f;
  float dz = 0.f;

  uint8_t uIdx = 0;

  switch(axis) { 
    case Axis::X:
      dy = halfH;
      dz = halfH;
      uIdx = 0;
      break;
    case Axis::Y:
      dx = halfH;
      dz = halfH;
      uIdx = 1;
      break;
    case Axis::Z:
      dx = halfH;
      dy = halfH;
      uIdx = 2;
      break;
    default:
      break;
  }

  int x0 = std::min(unsigned(floor((x[0] - dx) / h)), sz[0] - 1);
  int y0 = std::min(unsigned(floor((x[1] - dy) / h)), sz[1] - 1);
  int z0 = std::min(unsigned(floor((x[2] - dz) / h)), sz[2] - 1);

  float a1 = ((x[0]-dx) - x0 * h) / h;
  float b1 = ((x[1]-dy) - y0 * h) / h;
  float c1 = ((x[2]-dz) - z0 * h) / h;

  int x1 = std::min(x0 + 1, int(sz[0] - 1));
  int y1 = std::min(y0 + 1, int(sz[1] - 1));
  int z1 = std::min(z0 + 1, int(sz[2] - 1));

  float a0 = 1.0 - a1;
  float b0 = 1.0 - b1;
  float c0 = 1.0 - c1;

  float u000 = u(x0, y0, z0)[uIdx];
  float u001 = u(x0, y0, z1)[uIdx];
  float u010 = u(x0, y1, z0)[uIdx];
  float u011 = u(x0, y1, z1)[uIdx];
  float u100 = u(x1, y0, z0)[uIdx];
  float u101 = u(x1, y0, z1)[uIdx];
  float u110 = u(x1, y1, z0)[uIdx];
  float u111 = u(x1, y1, z1)[uIdx];

  // X
  float u00 = (a0 * u000) + (a1 * u100);
  float u10 = (a0 * u010) + (a1 * u110);
  float u01 = (a0 * u001) + (a1 * u101);
  float u11 = (a0 * u011) + (a1 * u111);

  // Y
  float u0 = (b0 * u00) + (b1 * u10);
  float u1 = (b0 * u01) + (b1 * u11);

  // Z
  return (c0 * u0) + (c1 * u1);
}

float Water::InterpSmoke(Vec3f& x) {
  const Vec3u& sz = u.GetSize();

  const float halfH = h / 2.0f;

  x[0] = std::max(std::min(x[0], sz[0] * h), h);
  x[1] = std::max(std::min(x[1], sz[1] * h), h);
  x[2] = std::max(std::min(x[2], sz[2] * h), h);

  float dx = halfH;
  float dy = halfH;
  float dz = halfH;

  int x0 = std::min(unsigned(floor((x[0] - dx) / h)), sz[0] - 1);
  int y0 = std::min(unsigned(floor((x[1] - dy) / h)), sz[1] - 1);
  int z0 = std::min(unsigned(floor((x[2] - dz) / h)), sz[2] - 1);

  float a1 = ((x[0]-dx) - x0 * h) / h;
  float b1 = ((x[1]-dy) - y0 * h) / h;
  float c1 = ((x[2]-dz) - z0 * h) / h;

  int x1 = std::min(x0 + 1, int(sz[0] - 1));
  int y1 = std::min(y0 + 1, int(sz[1] - 1));
  int z1 = std::min(z0 + 1, int(sz[2] - 1));

  float a0 = 1.0 - a1;
  float b0 = 1.0 - b1;
  float c0 = 1.0 - c1;

  float u000 = smoke(x0, y0, z0);
  float u001 = smoke(x0, y0, z1);
  float u010 = smoke(x0, y1, z0);
  float u011 = smoke(x0, y1, z1);
  float u100 = smoke(x1, y0, z0);
  float u101 = smoke(x1, y0, z1);
  float u110 = smoke(x1, y1, z0);
  float u111 = smoke(x1, y1, z1);

  // X
  float u00 = (a0 * u000) + (a1 * u100);
  float u10 = (a0 * u010) + (a1 * u110);
  float u01 = (a0 * u001) + (a1 * u101);
  float u11 = (a0 * u011) + (a1 * u111);

  // Y
  float u0 = (b0 * u00) + (b1 * u10);
  float u1 = (b0 * u01) + (b1 * u11);

  // Z
  return (c0 * u0) + (c1 * u1);
}

float Water::InterpPhi(const Vec3f& x) { 
  return -1; 
}