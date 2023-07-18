#include "FastSweep.h"
#include <iostream>

#define SWAP(fa, fb) \
  tmp = fa;          \
  fa = fb;           \
  fb = tmp;
using namespace std;

void Sort3f(float * arr) { 
  float tmp;
  
  if (arr[0] > arr[1]) {
    SWAP(arr[0], arr[1])
  }
  if (arr[1] > arr[2]) {
    SWAP(arr[1], arr[2])
  }
  if (arr[0] > arr[1]) {
    SWAP(arr[0], arr[1])
  }
}

void CloseExterior(Array3D<short>& dist, short far) {
  Vec3u gridSize = dist.GetSize();
  for (unsigned k = 0; k < gridSize[2]; k++) {
    for (unsigned j = 0; j < gridSize[1]; j++) {
      for (unsigned i = 0; i < gridSize[0]; i++) {
        short d = dist(i, j, k);
        if (d >= 0 && d < far) {
          // found a positive value enclosing this row
          break;
        }
        if (d < 0) {
          dist(i, j, k) = -d;
          break;
        }
      }
      for (unsigned i = 0; i < gridSize[0]; i++) {
        unsigned ix = gridSize[0] - i - 1;
        short d = dist(ix, j, k);
        if (d >= 0 && d < far) {
          // found a positive value enclosing this row
          break;
        }
        if (d < 0) {
          dist(ix, j, k) = -d;
          break;
        }
      }
    }
  }
  // along y
  for (unsigned k = 0; k < gridSize[2]; k++){
    for (unsigned i = 0; i < gridSize[0]; i++) {
      for (unsigned j = 0; j < gridSize[1]; j++) {
        short d = dist(i, j, k);
        if (d >= 0 && d < far) {
          // found a positive value enclosing this row
          break;
        }
        if (d < 0) {
          dist(i, j, k) = -d;
          break;
        }
      }
      for (unsigned j = 0; j < gridSize[1]; j++) {
        unsigned iy = gridSize[1] - j - 1;
        short d = dist(i, iy, k);
        if (d >= 0 && d < far) {
          // found a positive value enclosing this row
          break;
        }
        if (d < 0) {
          dist(i, iy, k) = -d;
          break;
        }
      }
    }
  }

  for (unsigned j = 0; j < gridSize[1]; j++) {
    for (unsigned i = 0; i < gridSize[0]; i++) {
      for (unsigned k = 0; k < gridSize[2]; k++) {
        short d = dist(i, j, k);
        if (d >= 0 && d < far) {
          // found a positive value enclosing this row
          break;
        }
        if (d < 0) {
          dist(i, j, k) = -d;
          break;
        }
      }
      for (unsigned k = 0; k < gridSize[2]; k++) {
        unsigned iz = gridSize[2] - k - 1;
        short d = dist(i, j, iz);
        if (d >= 0 && d < far) {
          // found a positive value enclosing this row
          break;
        }
        if (d < 0) {
          dist(i, j, iz) = -d;
          break;
        }
      }
    }
  }
}

void FastSweep(short* vals,unsigned N, float voxSize, float unit, float band)
{
  Array3D<short> arr(N, N, N);
  std::vector<short>& data = arr.GetData();
  for (size_t i = 0; i < data.size(); i++) {
    data[i] = vals[i];
  }
  FastSweep(arr, voxSize, unit, band);
  for (size_t i = 0; i < data.size(); i++) {
    vals[i] = data[i];
  }
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
  float h = voxSize / unit;
  float bandUnit = band * h;
  short far = bandUnit * 2;
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
          
          short minSignedVal = dist(ix, iy, iz);
          adjVal[0] = abs(minSignedVal);
          adjVal[1] = adjVal[0];
          adjVal[2] = adjVal[0];

          //take min neighbor in each direction
          if (ix < gridSize[0] - 1 && adjVal[0] > abs(dist(ix + 1, iy, iz))) {
            minSignedVal = dist(ix + 1, iy, iz);
            adjVal[0] = abs(minSignedVal);
          }
          if (ix > 0 && adjVal[0] > abs(dist(ix - 1, iy, iz))) {
            minSignedVal = dist(ix - 1, iy, iz);
            adjVal[0] = abs(minSignedVal);
          }

          if (iy < gridSize[1] - 1 && adjVal[1] > abs(dist(ix, iy + 1, iz))) {
            adjVal[1] = abs(dist(ix, iy + 1, iz));
            if (adjVal[1] < abs(minSignedVal)) {
              minSignedVal = dist(ix, iy + 1, iz);
            }
          }
          if (iy > 0 && adjVal[1] > abs(dist(ix, iy - 1, iz))) {
            adjVal[1] = abs(dist(ix, iy - 1, iz));
            if (adjVal[1] < abs(minSignedVal)) {
              minSignedVal = dist(ix, iy - 1, iz);
            }
          }
          
          if (iz < gridSize[2] - 1 && adjVal[2] > abs(dist(ix, iy, iz + 1))) {
            adjVal[2] = abs(dist(ix, iy, iz + 1));
            if (adjVal[2] < abs(minSignedVal)) {
              minSignedVal = dist(ix, iy, iz + 1);
            }
          }
          if (iz > 0 && adjVal[0] > abs(dist(ix, iy, iz-1))) {
            adjVal[2] = abs(dist(ix, iy, iz - 1));
            if (adjVal[2] < abs(minSignedVal)) {
              minSignedVal = dist(ix, iy, iz - 1);
            }
          }

          Sort3f(adjVal);
          if (adjVal[0] > bandUnit) {
            //no need to expand if closest val is further than narrow band
            continue;
          }
          float d_curr = adjVal[0] + h;
          float d_new;
          if (d_curr <= (adjVal[1])) {
            d_new = d_curr;  // accept the solution
          } else {
            // quadratic equation with coefficients involving 2 neighbor values
            // aa
            double a = 2.0;
            double b = -2.0 * (adjVal[0] + adjVal[1]);
            double c = adjVal[0] * adjVal[0] + adjVal[1] * adjVal[1] - h * h;
            double D = sqrt(b * b - 4.0 * a * c);
            // choose the minimal root
            d_curr = ((-b + D) > (-b - D) ? (-b + D) : (-b - D)) / (2.0 * a);

            if (d_curr <= (adjVal[2]))
              d_new = d_curr;  // accept the solution
            else {
              // quadratic equation with coefficients involving all 3 neighbor
              // values aa
              a = 3.0;
              b = -2.0 * (adjVal[0] + adjVal[1] + adjVal[2]);
              c = adjVal[0] * adjVal[0] + adjVal[1] * adjVal[1] +
                  adjVal[2] * adjVal[2] -
                  h * h;
              D = sqrt(b * b - 4.0 * a * c);
              // choose the minimal root
              d_new = ((-b + D) > (-b - D) ? (-b + D) : (-b - D)) / (2.0 * a);
            }
          }
          // update if d_new is smaller
          short sign = 1;
          if (minSignedVal < 0) {
            sign = -1;
          }
          if (abs(dist(ix, iy, iz)) > d_new) {
            dist(ix, iy, iz) = d_new * sign;
          }
        }//i
      }//j
    }//k    
  }//s
}