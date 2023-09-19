#include "Water.h"
#include <cmath>

int Water::Allocate(unsigned sx, unsigned sy, unsigned sz) {
  u.Allocate(sx + 1, sy + 1, sz + 1);
  uTmp.Allocate(sx + 1, sy + 1, sz + 1);
  phi.Allocate(sx + 1, sy + 1, sz + 1);
  p.Allocate(sx, sy, sz);

  return 0;
}

int Water::AdvectU() {
  const Vec3u sz = u.GetSize();
  for (int x = 0; x < sz[0]; ++x) {
    for (int y = 0; y < sz[1]; ++y) {
      for (int z = 0; z < sz[2]; ++z) {
        Vec3f& vPrev = u(x, y, z);

        Vec3f p(
           x - vPrev[0] * dt,
           y - vPrev[1] * dt,
           z - vPrev[2] * dt
        );

        uTmp(x, y, z) =  InterpU(p);
      }
    }
  } 

  u = uTmp;
  return 0;
}

int Water::AddBodyForce() {
  return -1;
}

int Water::SolveP() { 
        // Assuming the grids are cubic for simplicity
        Vec3u size = p.GetSize();

        if (size[0] == 0 || 
            size[1] == 0 || 
            size[2] == 0) return -1; // Error: grid size is zero

        // Temp grid to store intermediate results
        Array3D<float> p_temp(size[0], size[1], size[2]);
        
        float tolerance = 1e-4f;
        float max_iterations = 100;

        float hx = 1.0f / (size[0] - 1); // Assuming unit cube for simplicity
        float hy = 1.0f / (size[1] - 1); // Assuming unit cube for simplicity
        float hz = 1.0f / (size[2] - 1); // Assuming unit cube for simplicity

        float h2 = h * h;

        for(int iter = 0; iter < max_iterations; ++iter) {
            float max_error = 0.0f;

            for(int x = 1; x < size[0]; ++x) {
                for(int y = 1; y < size[1]; ++y) {
                    for(int z = 1; z < size[2]; ++z) {

                        float divergence = -0.5f * (
                            u(x+1, y, z)[0] - u(x-1,y,z)[0] +
                            u(x, y+1, z)[1] - u(x,y-1,z)[1] +
                            u(x, y, z+1)[2] - u(x,y,z-1)[2]
                        ) / h;

                        p_temp(x,y,z) = (1.0f / 6.0f) * (h2 * divergence +
                            p(x+1, y, z) + p(x-1, y, z) +
                            p(x, y+1, z) + p(x, y-1, z) +
                            p(x, y, z+1) + p(x, y, z-1));

                        max_error = std::max(max_error, std::fabs(p_temp(x,y,z) - p(x,y,z)));
                    }
                }
            }

            // Swap the grids
            memcpy(p.DataPtr(), p_temp.DataPtr(), size[0]*size[1]*size[2]*sizeof(p(0,0,0)));

            // Check for convergence
            if (max_error < tolerance) {
                return 0; // Success: the solution has converged
            }
        }

        return 1; //
}

int Water::AdvectPhi() { return 0; }

int Water::Step() {
  AdvectU();
  AddBodyForce();
  SolveP();
  AdvectPhi();
  return 0;
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

float Water::InterpPhi(const Vec3f& x) { 
  return -1; 
}