#include "FastSweep.h"

#include <execution>
#include <iostream>

#define SWAP(fa, fb) \
  tmp = fa;          \
  fa = fb;           \
  fb = tmp;
using namespace std;

void Sort3f(float* arr) {
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
  for (unsigned k = 0; k < gridSize[2]; k++) {
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

template <unsigned N>
void FastSweep(FixedGrid3D<N>& dist, float voxSize, float unit, float band,
               Array3D<uint8_t>& frozen) {
  if (N == 0) {
    return;
  }
  if (frozen.Empty()) {
    frozen.Allocate(Vec3u(N, N, N), 0);
  }
  // cells initialized with dist < h are frozen
  const short* src = dist.val;
  std::vector<uint8_t>& dst = frozen.GetData();
  float h = voxSize / unit;
  float bandUnit = band * h;
  for (size_t i = 0; i < dst.size(); i++) {
    if (std::abs(src[i]) <= h) {
      dst[i] = true;
    }
  }

  const unsigned NSweeps = 8;
  const int DIRS[NSweeps][3] = {{-1, -1, 1}, {1, -1, 1},   {1, 1, 1},
                                {-1, 1, 1},  {-1, -1, -1}, {1, -1, -1},
                                {1, 1, -1},  {-1, 1, -1}};
  // adjacent values.
  float adjVal[3];
  for (unsigned s = 0; s < NSweeps; s++) {
    for (unsigned k = 0; k < N; k++) {
      unsigned iz = DIRS[s][2] > 0 ? k : (N - k - 1);
      for (unsigned j = 0; j < N; j++) {
        unsigned iy = DIRS[s][1] > 0 ? j : (N - j - 1);
        for (unsigned i = 0; i < N; i++) {
          unsigned ix = DIRS[s][0] > 0 ? i : (N - i - 1);
          if (frozen(ix, iy, iz)) {
            continue;
          }

          short minSignedVal = dist(ix, iy, iz);
          adjVal[0] = abs(minSignedVal);
          adjVal[1] = adjVal[0];
          adjVal[2] = adjVal[0];

          // take min neighbor in each direction
          if (ix < N - 1 && adjVal[0] > abs(dist(ix + 1, iy, iz))) {
            minSignedVal = dist(ix + 1, iy, iz);
            adjVal[0] = abs(minSignedVal);
          }
          if (ix > 0 && adjVal[0] > abs(dist(ix - 1, iy, iz))) {
            minSignedVal = dist(ix - 1, iy, iz);
            adjVal[0] = abs(minSignedVal);
          }

          if (iy < N - 1 && adjVal[1] > abs(dist(ix, iy + 1, iz))) {
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

          if (iz < N - 1 && adjVal[2] > abs(dist(ix, iy, iz + 1))) {
            adjVal[2] = abs(dist(ix, iy, iz + 1));
            if (adjVal[2] < abs(minSignedVal)) {
              minSignedVal = dist(ix, iy, iz + 1);
            }
          }
          if (iz > 0 && adjVal[0] > abs(dist(ix, iy, iz - 1))) {
            adjVal[2] = abs(dist(ix, iy, iz - 1));
            if (adjVal[2] < abs(minSignedVal)) {
              minSignedVal = dist(ix, iy, iz - 1);
            }
          }
          short sign = 1;
          if (minSignedVal < 0) {
            sign = -1;
          }
          Sort3f(adjVal);
          if (adjVal[0] > bandUnit) {
            // no need to expand if closest val is further than narrow band
            continue;
          }
          float d_curr = adjVal[0] + h;
          float d_new;
          if (d_curr <= (adjVal[1])) {
            d_new = d_curr;  // accept the solution
          } else {
            // quadratic equation with coefficients involving 2 neighbor values
            // aa
            float a = 2.0f;
            float b = -2.0f * (adjVal[0] + adjVal[1]);
            float c = adjVal[0] * adjVal[0] + adjVal[1] * adjVal[1] - h * h;
            float D = sqrt(b * b - 4.0f * a * c);
            // choose the minimal root
            d_curr = ((-b + D) > (-b - D) ? (-b + D) : (-b - D)) / (2.0f * a);

            if (d_curr <= (adjVal[2]))
              d_new = d_curr;  // accept the solution
            else {
              // quadratic equation with coefficients involving all 3 neighbor
              // values aa
              a = 3.0f;
              b = -2.0f * (adjVal[0] + adjVal[1] + adjVal[2]);
              c = adjVal[0] * adjVal[0] + adjVal[1] * adjVal[1] +
                  adjVal[2] * adjVal[2] - h * h;
              D = sqrt(b * b - 4.0f * a * c);
              // choose the minimal root
              d_new = ((-b + D) > (-b - D) ? (-b + D) : (-b - D)) / (2.0f * a);
            }
          }
          // update if d_new is smaller
          if (abs(dist(ix, iy, iz)) > d_new) {
            dist(ix, iy, iz) = d_new * sign;
          }
        }  // i
      }    // j
    }      // k
  }        // s
}

template void FastSweep<5>(FixedGrid3D<5>& grid, float voxSize, float unit,
                           float band, Array3D<uint8_t>& frozen);
namespace Sweep {
const unsigned NSweeps = 8;
const int DIRS[NSweeps][3] = {{-1, -1, 1}, {1, -1, 1},   {1, 1, 1},
                              {-1, 1, 1},  {-1, -1, -1}, {1, -1, -1},
                              {1, 1, -1},  {-1, 1, -1}};
}  // namespace Sweep

void SolveOnePoint(unsigned ix, unsigned iy, unsigned iz, Array3D<short>& dist,
                   float h, float band, const Array3D<uint8_t>& frozen) {
  if (frozen(ix, iy, iz)) {
    return;
  }
  Vec3u gridSize = dist.GetSize();
  // adjacent values.
  float adjVal[3];
  short minSignedVal = dist(ix, iy, iz);
  adjVal[0] = abs(minSignedVal);
  adjVal[1] = adjVal[0];
  adjVal[2] = adjVal[0];

  // take min neighbor in each direction
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
  if (iz > 0 && adjVal[0] > abs(dist(ix, iy, iz - 1))) {
    adjVal[2] = abs(dist(ix, iy, iz - 1));
    if (adjVal[2] < abs(minSignedVal)) {
      minSignedVal = dist(ix, iy, iz - 1);
    }
  }
  short sign = 1;
  if (minSignedVal < 0) {
    sign = -1;
  }
  Sort3f(adjVal);
  if (adjVal[0] > band * h) {
    // no need to expand if closest val is further than narrow band
    return;
  }
  float d_curr = adjVal[0] + h;
  float d_new;
  if (d_curr <= (adjVal[1])) {
    d_new = d_curr;  // accept the solution
  } else {
    // quadratic equation with coefficients involving 2 neighbor values
    // aa
    float a = 2.0f;
    float b = -2.0f * (adjVal[0] + adjVal[1]);
    float c = adjVal[0] * adjVal[0] + adjVal[1] * adjVal[1] - h * h;
    float D = sqrt(b * b - 4.0f * a * c);
    // choose the minimal root
    d_curr = ((-b + D) > (-b - D) ? (-b + D) : (-b - D)) / (2.0f * a);

    if (d_curr <= (adjVal[2]))
      d_new = d_curr;  // accept the solution
    else {
      // quadratic equation with coefficients involving all 3 neighbor
      // values aa
      a = 3.0f;
      b = -2.0f * (adjVal[0] + adjVal[1] + adjVal[2]);
      c = adjVal[0] * adjVal[0] + adjVal[1] * adjVal[1] +
          adjVal[2] * adjVal[2] - h * h;
      D = sqrt(b * b - 4.0f * a * c);
      // choose the minimal root
      d_new = ((-b + D) > (-b - D) ? (-b + D) : (-b - D)) / (2.0f * a);
    }
  }
  // update if d_new is smaller
  if (abs(dist(ix, iy, iz)) > d_new) {
    dist(ix, iy, iz) = d_new * sign;
  }
}

void SweepOneDir(unsigned dir, Array3D<short>& dist, float h, float band,
                 Array3D<uint8_t>& frozen) {
  Vec3u gridSize = dist.GetSize();
  size_t count = 0;
  for (unsigned k = 0; k < gridSize[2]; k++) {
    unsigned iz = Sweep::DIRS[dir][2] > 0 ? k : (gridSize[2] - k - 1);
    for (unsigned j = 0; j < gridSize[1]; j++) {
      unsigned iy = Sweep::DIRS[dir][1] > 0 ? j : (gridSize[1] - j - 1);
      for (unsigned i = 0; i < gridSize[0]; i++) {
        unsigned ix = Sweep::DIRS[dir][0] > 0 ? i : (gridSize[0] - i - 1);
        SolveOnePoint(ix, iy, iz, dist, h, band, frozen);
      }  // i
    }    // j
  }      // k
}

void FastSweep(Array3D<short>& dist, float voxSize, float unit, float band,
               Array3D<uint8_t>& frozen) {
  Vec3u gridSize = dist.GetSize();
  if (gridSize[0] == 0 || gridSize[1] == 0 || gridSize[2] == 0) {
    return;
  }
  if (frozen.Empty()) {
    frozen.Allocate(gridSize, 0);
  }
  // cells initialized with dist < h are frozen
  const std::vector<short>& src = dist.GetData();
  std::vector<uint8_t>& dst = frozen.GetData();
  float h = voxSize / unit;
  for (size_t i = 0; i < src.size(); i++) {
    if (std::abs(src[i]) <= h) {
      dst[i] = true;
    }
  }
  for (unsigned s = 0; s < Sweep::NSweeps; s++) {
    SweepOneDir(s, dist, h, band, frozen);
  }
}

// parallel stuff

/// <summary>
/// direction 0: x+y+z = level
/// 1: (size[0] - x - 1) + y + z = level
/// </summary>
/// <param name="gridSize"></param>
/// <param name="level"> max is size[0]+size[1]+size[2] - 3</param>
/// <param name="dir">direction 0-7</param>
/// <returns>list of integer indices</returns>
void GetLevelRangeZ(const Vec3u& size, unsigned level, unsigned dir,
                    unsigned& k0, unsigned& k1) {
  if (size[0] == 0 || size[1] == 0 || size[2] == 0) {
    return;
  }
  k0 = 0;
  unsigned maxXY = size[0] + size[1] - 2;
  if (level > maxXY) {
    k0 = level - maxXY;
  }
  k1 = std::min(size[2] - 1, level);
}

class SweepArgs {
 public:
  SweepArgs(unsigned dir, unsigned level, unsigned k0, unsigned k1,
            Array3D<short>* dist, float h, float band,
            const Array3D<uint8_t>* frozen)
      : _dir(dir),
        _level(level),
        _k0(k0),
        _k1(k1),
        _dist(dist),
        _h(h),
        _band(band),
        _frozen(frozen) {}
  unsigned _dir = 0;
  unsigned _level = 0;
  unsigned _k0 = 0, _k1 = 0;
  Array3D<short>* _dist = nullptr;
  float _h = 0.0f;
  float _band = 0.0f;
  const Array3D<uint8_t>* _frozen = nullptr;
};

void SweepLevelTask(SweepArgs* a) {
  Vec3u gridSize = a->_dist->GetSize();
  unsigned maxx = gridSize[0] - 1;
  for (unsigned k = a->_k0; k <= a->_k1; k++) {
    unsigned z = (a->_dir & 0x4) ? (gridSize[2] - k - 1) : k;
    unsigned xySum = a->_level - k;
    unsigned j0 = 0;
    if (xySum > maxx) {
      j0 = xySum - maxx;
    }
    unsigned j1 = std::min(gridSize[1] - 1, xySum);

    for (unsigned j = j0; j <= j1; j++) {
      unsigned i = xySum - j;
      unsigned y = (a->_dir & 0x2) ? (gridSize[1] - j - 1) : j;
      unsigned x = (a->_dir & 0x1) ? (gridSize[0] - i - 1) : i;
      SolveOnePoint(x, y, z, *a->_dist, a->_h, a->_band, *a->_frozen);
    }
  }
}

void SweepOneDiag(unsigned dir, Array3D<short>& dist, float h, float band,
                  Array3D<uint8_t>& frozen) {
  Vec3u gridSize = dist.GetSize();
  unsigned maxLevel = gridSize[0] + gridSize[1] + gridSize[2] - 3;
  unsigned maxx = gridSize[0] - 1;

  const unsigned NUM_TASKS = 32;

  for (unsigned level = 0; level <= maxLevel; level++) {
    unsigned k0 = 0, k1 = 0;
    GetLevelRangeZ(gridSize, level, dir, k0, k1);
    unsigned numNodes = 0;
    std::vector<unsigned> rowSize(k1 - k0 + 1, 0);
    for (unsigned k = k0; k <= k1; k++) {
      unsigned z = (dir & 0x4) ? (gridSize[2] - k - 1) : k;
      unsigned xySum = level - k;
      unsigned j0 = 0;
      if (xySum > maxx) {
        j0 = xySum - maxx;
      }
      unsigned j1 = std::min(gridSize[1] - 1, xySum);
      numNodes += (j1 - j0 + 1);
      rowSize[k - k0] = numNodes;
    }
    if (numNodes < 4 * NUM_TASKS) {
      for (unsigned k = k0; k <= k1; k++) {
        unsigned z = (dir & 0x4) ? (gridSize[2] - k - 1) : k;
        unsigned xySum = level - k;
        unsigned j0 = 0;
        if (xySum > maxx) {
          j0 = xySum - maxx;
        }
        unsigned j1 = std::min(gridSize[1] - 1, xySum);

        for (unsigned j = j0; j <= j1; j++) {
          unsigned i = xySum - j;
          unsigned y = (dir & 0x2) ? (gridSize[1] - j - 1) : j;
          unsigned x = (dir & 0x1) ? (gridSize[0] - i - 1) : i;
          SolveOnePoint(x, y, z, dist, h, band, frozen);
        }
      }
      continue;
    }
    unsigned batchSize = numNodes / NUM_TASKS;
    unsigned prevK = 0;
    std::vector<SweepArgs> args;
    for (unsigned t = 0; t < NUM_TASKS; t++) {
      unsigned i = prevK;
      for (; i < rowSize.size(); i++) {
        if (rowSize[i] >= batchSize * (t + 1)) {
          break;
        }
      }
      if (t == NUM_TASKS - 1) {
        i = k1 - k0;
      }

      args.push_back(
          SweepArgs(dir, level, prevK + k0, i + k0, &dist, h, band, &frozen));

      if (i + 1 >= rowSize.size()) {
        break;
      }
      prevK = i + 1;
    }
    std::for_each(std::execution::par_unseq, args.begin(), args.end(),
                  [](SweepArgs& a) { SweepLevelTask(&a); });
  }
}

// A parallel fast sweeping method for the Eikonal equation
//   Miles Detrixhe, Frederic Gibou, Chohong Min
//  sweep diagonally from 8 corners of the grid.
void FastSweepPar(Array3D<short>& dist, float voxSize, float unit, float band,
                  Array3D<uint8_t>& frozen) {
  Vec3u gridSize = dist.GetSize();
  if (gridSize[0] == 0 || gridSize[1] == 0 || gridSize[2] == 0) {
    return;
  }
  if (frozen.Empty()) {
    frozen.Allocate(gridSize, 0);
  }
  // cells initialized with dist < h are frozen
  const std::vector<short>& src = dist.GetData();
  std::vector<uint8_t>& dst = frozen.GetData();
  float h = voxSize / unit;
  for (size_t i = 0; i < src.size(); i++) {
    if (std::abs(src[i]) <= h) {
      dst[i] = true;
    }
  }
  for (unsigned s = 0; s < Sweep::NSweeps; s++) {
    SweepOneDiag(s, dist, h, band, frozen);
  }
}
