#include "FastSweep.h"
void FastSweep3D(double* grid, bool* frozenCells, GridParams* gridPar) {
  /* ... */
  /* initialization & iterating from 8 corners */

  uint gridPos = ((iz * Ny + iy) * Nx + ix);

  if (!frozenCells[gridPos]) {
  }
    // === neighboring cells (Upwind Godunov) ...
    // ... same as for 2D image, but with all domain walls & corners ===
    // ...
    // simple bubble sort
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

    double d_curr =
        aa[0] + h * f;  // just a linear equation with the first neighbor value
    double d_new;
    if (d_curr <= (aa[1] + eps)) {
      d_new = d_curr;  // accept the solution
    } else {
      // quadratic equation with coefficients involving 2 neighbor values aa
      double a = 2.0;
      double b = -2.0 * (aa[0] + aa[1]);
      double c = aa[0] * aa[0] + aa[1] * aa[1] - h * h * f * f;
      double D = sqrt(b * b - 4.0 * a * c);
      // choose the minimal root
      d_curr = ((-b + D) > (-b - D) ? (-b + D) : (-b - D)) / (2.0 * a);

      if (d_curr <= (aa[2] + eps))
        d_new = d_curr;  // accept the solution
      else {
        // quadratic equation with coefficients involving all 3 neighbor values
        // aa
        a = 3.0;
        b = -2.0 * (aa[0] + aa[1] + aa[2]);
        c = aa[0] * aa[0] + aa[1] * aa[1] + aa[2] * aa[2] - h * h * f * f;
        D = sqrt(b * b - 4.0 * a * c);
        // choose the minimal root
        d_new = ((-b + D) > (-b - D) ? (-b + D) : (-b - D)) / (2.0 * a);
      }
    }
    // update if d_new is smaller
    grid[gridPos] = grid[gridPos] < d_new ? grid[gridPos] : d_new
  

  /* for-loops scope ends */
  /* ... */
}