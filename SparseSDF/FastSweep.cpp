#include "FastSweep.h"

void Sort3f(float * arr) { 
  float tmp;
  #define SWAP(fa, fb) tmp=fa;fa=fb;fb=tmp;
  if (arr[0] > arr[1]) {
    SWAP(arr[0], arr[1]);
  }
  if (arr[1] > arr[2]) {
    SWAP(arr[1], arr[2]);
  }
  if (arr[0] > arr[1]) {
    SWAP(arr[0], arr[1]);
  }
  #undef SWAP
}

void FastSweep(Array3D<short>& dist, float voxSize, float unit, float band)
{
  
  Vec3u gridSize = dist.GetSize();
  if (gridSize[0] == 0 || gridSize[1] == 0 || gridSize[2] == 0) {
    return;
  }
  Array3D<uint8_t> frozen;
  frozen.Allocate(gridSize, 0);
  //cells initialized with dist < MAX_DIST are frozen
  const std::vector<short>& src = dist.GetData();
  std::vector<uint8_t>& dst = frozen.GetData();
  short far = band * 2 / unit;
  for (size_t i = 0; i < src.size(); i++) {
    if (src[i] < far) {
      dst[i] = true;
    }
  }
  const unsigned NSweeps = 8;
  const int DIRS[NSweeps][3] = {{-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1},
                                {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}};
  // adjacent values.
  float adjVal[3];
  float h = voxSize;
  for (unsigned s = 0; s < NSweeps; s++) {
    for (unsigned k = 0; k < gridSize[2]; k++) {
      unsigned iz = DIRS[s][2] > 0 ? k : (gridSize[2] - k - 1);
      for (unsigned j = 0; j < gridSize[1]; j++) {
        unsigned iy = DIRS[s][1] > 0 ? j : (gridSize[1] - j - 1);
        for (unsigned i = 0; i < gridSize[0]; i++) {
          unsigned ix = DIRS[s][0] > 0 ? i : (gridSize[0] - i - 1);
          if (frozen(ix,iy,iz)) {
            continue;
          }

          //take min neighbor in each direction
          if (ix == 0) {
            adjVal[0] = dist(ix + 1, iy, iz);
          } else if (ix == gridSize[0] - 1) {
            adjVal[0] = dist(ix - 1, iy, iz);
          } else {
            adjVal[0] = std::min(dist(ix - 1, iy, iz), dist(ix + 1, iy, iz));
          }

          if (iy == 0) {
            adjVal[1] = dist(ix, iy + 1, iz);
          } else if (iy == gridSize[1] - 1) {
            adjVal[1] = dist(ix, iy - 1, iz);
          } else {
            adjVal[1] = std::min(dist(ix, iy - 1, iz), dist(ix, iy + 1, iz));
          }

          if (iz == 0) {
            adjVal[2] = dist(ix, iy, iz + 1);
          } else if (iz == gridSize[2] - 1) {
            adjVal[2] = dist(ix, iy, iz - 1);
          } else {
            adjVal[2] = std::min(dist(ix, iy, iz - 1), dist(ix, iy, iz + 1));
          }
          Sort3f(adjVal);


          double d_curr = aa[0] + h;
          double d_new;
          if (d_curr <= (aa[1] + eps)) {
            d_new = d_curr;  // accept the solution
          } else {
            // quadratic equation with coefficients involving 2 neighbor values
            // aa
            double a = 2.0;
            double b = -2.0 * (aa[0] + aa[1]);
            double c = aa[0] * aa[0] + aa[1] * aa[1] - h * h;
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
              c = aa[0] * aa[0] + aa[1] * aa[1] + aa[2] * aa[2] - h * h;
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