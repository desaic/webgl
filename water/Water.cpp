#include "Water.h"
#include <cmath>
#include <iostream>

int Water::Allocate(unsigned sx, unsigned sy, unsigned sz) {
  u.Allocate(sx + 1, sy + 1, sz + 1);
  uTmp.Allocate(sx + 1, sy + 1, sz + 1);
  phi.Allocate(sx + 1, sy + 1, sz + 1);
  p.Allocate(sx, sy, sz);

  return 0;
}

int Water::Initialize() {
  Allocate(3, 3, 3);
  return 0;
}

int Water::InitializeSimple() {
  Allocate(3,3,3);
  const Vec3u& sz = u.GetSize();
  for (int i = 1; i < sz[0] - 1; ++i) {
      for (int j = 1; j < sz[1] - 1; ++j) {
        for (int k = 1; k < sz[2] - 1; ++k) {
          u(i, j, k) = Vec3f(1.0f, 0.f, 0.f);
        }
      }
  }

  return 0;
}

int Water::AdvectU() {
  const float h2 = h / 2.0;
  const Vec3u& sz = u.GetSize();
  for (int i = 0; i < sz[0]; ++i) {
    for (int j = 0; j < sz[1]; ++j) {
      for (int k = 0; k < sz[2]; ++k) {
        if (boundary(i, j, k) == 0) continue;

        //std::cout << "update " << i << "," << j << "," << k << std::endl;
        // x
        if (i - 1 >= 0 && j + 1 < sz[1] && k + 1 < sz[2]) {
          float x = i*h;
          float y = j*h + h2;
          float z = k*h + h2;

          float uX = u(i, j, k)[0];
          float uY = AvgU_y(i, j, k);
          float uZ = AvgU_z(i, j, k);

          x -= dt * uX;
          y -= dt * uY;
          z -= dt * uZ;

          uTmp(i, j, k)[0] = InterpU(Vec3f(x, y, z))[0];
          //std::cout << "update x " <<  uTmp(i, j, k)[0] << std::endl;
        }

        // y
        if (j - 1 >= 0 && i + 1 < sz[0] && k + 1 < sz[2]) {
          float x = i*h + h2;
          float y = j*h;
          float z = k*h + h2;

          float uX = AvgV_x(i, j, k);
          float uY = u(i, j, k)[0];
          float uZ = AvgV_z(i, j, k);
  
          x -= dt * uX;
          y -= dt * uY;
          z -= dt * uZ;

          uTmp(i, j, k)[1] = InterpU(Vec3f(x, y, z))[1];
          //std::cout << "update y " << uTmp(i, j, k)[1] << std::endl;
        }

        // z
        if (k - 1 >= 0 && i + 1 < sz[0] && j + 1 < sz[1]) {
          float x = i*h + h2;
          float y = j*h + h2;
          float z = k*h;

          float uX = AvgW_x(i, j, k);
          float uY = AvgW_y(i, j, k);
          float uZ = u(i, j, k)[2];
          
          x -= dt * uX;
          y -= dt * uY;
          z -= dt * uZ;

          uTmp(i, j, k)[1] = InterpU(Vec3f(x, y, z))[2];
          //std::cout << "update z " << uTmp(i, j, k)[2] << std::endl;
        }
      }
    }
  } 

  u = uTmp;
  return 0;
}

int Water::AddBodyForce() {
    const Vec3u& sz = u.GetSize();
    for (int i = 1; i < sz[0]; ++i) {
      for (int j = 0; j < sz[1]; ++j) {
        for (int k = 0; k < sz[2]; ++k) {
          u(i, j, k)[1] += gravity * dt;
        }  
      }
    }

    return 0;
}

// adapted from https://github.com/matthias-research/pages/blob/master/tenMinutePhysics/17-fluidSim.html
int Water::SolveP() { 
    const Vec3u& size = p.GetSize();

    if (size[0] == 0 || 
        size[1] == 0 || 
        size[2] == 0) return -1; // empty grid
        
    float tolerance = 1e-4f;
    float cp = density * h / dt;

    for (int iter = 0; iter < nIterations; ++iter) {
        // right now checks for worst case in a single cell
        float max_error = 0.0f; 

        for (int x = 1; x < size[0] - 1; ++x) {
            for (int y = 1; y < size[1] - 1; ++y) {
                for (int z = 1; z < size[2] - 1; ++z) {
                    if (boundary(x,y,z) < 1e-5) continue;

                    float sx0 = boundary(x-1, y  , z  );
                    float sx1 = boundary(x+1, y  , z  );
                    float sy0 = boundary(x  , y-1, z  );
                    float sy1 = boundary(x  , y+1, z  );
                    float sz0 = boundary(x  , y  , z-1);
                    float sz1 = boundary(x  , y  , z+1);

                    float s = sx0 + sx1 + sy0 + sy1 + sz0 + sz1;

                    if (s < 1e-5) continue;

                    float divergence = (
                        u(x+1, y  , z  )[0] - u(x, y, z)[0] +
                        u(x  , y+1, z  )[1] - u(x, y, z)[1] +
                        u(x  , y  , z+1)[2] - u(x, y, z)[2]
                    );

                    float pressure = overrelaxation* (-divergence / s);
                        
                    float p_0 = p(x, y, z);
                    p(x, y, z) = pressure * cp;

                    // Update velocity field, i think b/c gauss-seidel does in-place substitutions?
                    u(x  , y  , z  )[0] -= pressure * sx0;
                    u(x+1, y  , z  )[0] += pressure * sx1;
                    u(x  , y  , z  )[1] -= pressure * sy0;
                    u(x  , y+1, z  )[1] += pressure * sy1;                        
                    u(x  , y  , z  )[2] -= pressure * sz0;
                    u(x  , y  , z+1)[2] += pressure * sz1;

                    max_error = std::max(max_error, std::fabs(p_0 - p(x,y,z)));
                }
            }
        }
        if (max_error < tolerance) {
            return 0; 
        }
    }

    return -1; // did not converge 
}

int Water::AdvectPhi() { return 0; }

int Water::Step() {
  AdvectU();
  AddBodyForce();
  SolveP();
  AdvectPhi();
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
    u(i,   j,   k)[2] +
    u(i-1, j,   k)[2] +
    u(i,   j, k+1)[2] +
    u(i-1, j, k+1)[2]
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
    u(i,   j,   k)[2] +
    u(i, j-1,   k)[2] +
    u(i,   j, k+1)[2] +
    u(i, j-1, k+1)[2]
  );
}

float Water::AvgW_x(unsigned i, unsigned j, unsigned k) {
  return 0.25f * ( 
    u(i,   j,   k)[0] +
    u(i+1, j,   k)[0] +
    u(i,   j, k-1)[0] +
    u(i+1, j, k-1)[0]
  );
}

float Water::AvgW_y(unsigned i, unsigned j, unsigned k) {
  return 0.25f * ( 
    u(i,   j,   k)[0] +
    u(i, j+1,   k)[0] +
    u(i,   j, k-1)[0] +
    u(i, j+1, k-1)[0]
  );
}

Vec3f Water::InterpU(const Vec3f& x) { 
  int x0 = floor(x[0]);
  int x1 = x0 + 1;
  int y0 = floor(x[1]);
  int y1 = y0 + 1;
  int z0 = floor(x[2]);
  int z1 = z0 + 1;

  float a1 = x[0] - x0;
  float b1 = x[1] - y0;
  float c1 = x[2] - z0;

  float a0 = 1.0 - a1;
  float b0 = 1.0 - b1;
  float c0 = 1.0 - c1;

  Vec3f& u000 = u(x0, y0, z0);
  Vec3f& u001 = u(x0, y0, z1);
  Vec3f& u010 = u(x0, y1, z0);
  Vec3f& u011 = u(x0, y1, z1);
  Vec3f& u100 = u(x1, y0, z0);
  Vec3f& u101 = u(x1, y0, z1);
  Vec3f& u110 = u(x1, y1, z0);
  Vec3f& u111 = u(x1, y1, z1);

  // X
  Vec3f u00(
    u000[0] * a0 + u100[0] * a1,
    u000[1] * a0 + u100[1] * a1,
    u000[2] * a0 + u100[2] * a1
  );
  Vec3f u10(
    u010[0] * a0 + u110[0] * a1,
    u010[1] * a0 + u110[1] * a1,
    u010[2] * a0 + u110[2] * a1
  );
  Vec3f u01(
    u001[0] * a0 + u101[0] * a1,
    u001[1] * a0 + u101[1] * a1,
    u001[2] * a0 + u101[2] * a1
  );
  Vec3f u11(
    u011[0] * a0 + u111[0] * a1,
    u011[1] * a0 + u111[1] * a1,
    u011[2] * a0 + u111[2] * a1
  );

  // Y
  Vec3f u0(
    u00[0] * b0 + u10[0] * b1,
    u00[1] * b0 + u10[1] * b1,
    u00[2] * b0 + u10[2] * b1
  );
  Vec3f u1(
    u01[0] * b0 + u11[0] * b1,
    u01[1] * b0 + u11[1] * b1,
    u01[2] * b0 + u11[2] * b1
  );

  // Z
  return Vec3f(
    u0[0] * c0 + u1[0] * c1,
    u0[1] * c0 + u1[1] * c1,
    u0[2] * c0 + u1[2] * c1
  );
}

int Water::boundary(unsigned x, unsigned y, unsigned z) {
  const Vec3u sz = u.GetSize();
  if (x == 0 || x == sz[0] - 1 ||
      y == 0 || y == sz[1] - 1 || 
      z == 0 || z == sz[2] - 1) {
    return 0;
  }

  return 1;
}

float Water::InterpPhi(const Vec3f& x) { 
  return -1; 
}