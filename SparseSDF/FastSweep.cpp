#include "FastSweep.h"

void FastSweep3D(Array3D<short>& dist, const Array3D<uint8_t>& frozenCells,
                 Vec3f unit, float band) {

  const unsigned NSweeps = 8;
  const int DIRS[NSweeps][3] = {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1},
                                {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}};
  Vec3u gridSize = dist.GetSize();
  for (unsigned s = 0; s < NSweeps; s++) {
    for (unsigned k = 0; k < gridSize[2]; k++) {
      unsigned iz = DIRS[s][2] > 0 ? k : (gridSize[2] - k - 1);
      for (unsigned j = 0; j < gridSize[1]; j++) {
        unsigned iy = DIRS[s][1] > 0 ? j : (gridSize[1] - j - 1);
        for (unsigned i = 0; i < gridSize[0]; i++) {
          unsigned ix = DIRS[s][0] > 0 ? i : (gridSize[0] - i - 1);
          if (frozenCells(ix,iy,iz)) {
            continue;
          }

          if (aa[0] > aa[1]) {
            tmp = aa[0];
            aa[0] = aa[1];
            aa[1] = tmp;
          }
          if (aa[1] > aa[2]) {
            tmp = aa[1];
            aa[1] = aa[2];
            aa[2] = tmp;
          }
          if (aa[0] > aa[1]) {
            tmp = aa[0];
            aa[0] = aa[1];
            aa[1] = tmp;
          }

          double d_curr = aa[0] + h * f;
          double d_new;
          if (d_curr <= (aa[1] + eps)) {
            d_new = d_curr;  // accept the solution
          } else {
            // quadratic equation with coefficients involving 2 neighbor values
            // aa
            double a = 2.0;
            double b = -2.0 * (aa[0] + aa[1]);
            double c = aa[0] * aa[0] + aa[1] * aa[1] - h * h * f * f;
            double D = sqrt(b * b - 4.0 * a * c);
            // choose the minimal root
            d_curr = ((-b + D) > (-b - D) ? (-b + D) : (-b - D)) / (2.0 * a);

            if (d_curr <= (aa[2] + eps))
              d_new = d_curr;  // accept the solution
            else {
              // quadratic equation with coefficients involving all 3 neighbor
              // values aa
              a = 3.0;
              b = -2.0 * (aa[0] + aa[1] + aa[2]);
              c = aa[0] * aa[0] + aa[1] * aa[1] + aa[2] * aa[2] - h * h * f * f;
              D = sqrt(b * b - 4.0 * a * c);
              // choose the minimal root
              d_new = ((-b + D) > (-b - D) ? (-b + D) : (-b - D)) / (2.0 * a);
            }
          }
          // update if d_new is smaller
          grid[gridPos] = grid[gridPos] < d_new ? grid[gridPos] : d_new;

        }//i
      }//j
    }//k    

  }//s
}