#include "FloodOutside.h"
#include "Vec3.h"
#include <deque>

static bool InBound(int ix, int iy, int iz, const Vec3u &size) {
  return (ix >= 0) && (iy >= 0) && (iz >= 0) && ix < int(size[0]) && iy < int(size[1]) && iz < int(size[2]);
}

size_t LinearIdx(unsigned x, unsigned y, unsigned z, const Vec3u& size) {
  return x + y * size_t(size[0]) + z * size_t(size[0] * size[1]);
}

Vec3u GridIdx(size_t l, const Vec3u& size) {
  unsigned x = l % size[0];
  unsigned y = (l % (size[0] * size[1])) / size[0];
  unsigned z = l / (size[0] * size[1]);
  return Vec3u(x, y, z);
}

void FloodOutsideSeed(Vec3u seed, const Array3D<short>& dist, float distThresh, Array3D8u& label) {
  Vec3u size = dist.GetSize();
  size_t linearSeed = LinearIdx(seed[0], seed[1], seed[2], size);
  std::deque<size_t> q(1, linearSeed);
  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  while (!q.empty()) {
    size_t linearIdx = q.front();
    q.pop_front();
    Vec3u idx = GridIdx(linearIdx, size);
    label(idx[0], idx[1], idx[2]) = 1;
    for (unsigned ni = 0; ni < NUM_NBR; ni++) {
      int nx = int(idx[0] + nbrOffset[ni][0]);
      int ny = int(idx[1] + nbrOffset[ni][1]);
      int nz = int(idx[2] + nbrOffset[ni][2]);
      if (!InBound(nx, ny, nz, size)) {
        continue;
      }
      unsigned ux = unsigned(nx), uy = unsigned(ny), uz = unsigned(nz);
      uint8_t nbrLabel = label(ux, uy, uz);
      size_t nbrLinear = LinearIdx(ux, uy, uz, size);
      if (nbrLabel == 0) {
        uint16_t nbrDist = dist(ux, uy, uz);
        if (nbrDist >= distThresh) {
          label(ux, uy, uz) = 1;
          q.push_back(nbrLinear);
        }
      }
    }
  }
}

/// <param name="dist">distance grid. at least 1 voxel of padding is assumed.</param>
/// <param name="distThresh">stop flooding if distance is less than this</param>
/// <returns>voxel labels. 1 for outside.</returns>
Array3D8u FloodOutside(const Array3D<short>& dist, float distThresh) {
  Array3D8u label;
  Vec3u size = dist.GetSize();
  label.Allocate(size[0], size[1], size[2]);
  label.Fill(0);
  // process easy voxels first.

  // all 6 faces are labeled to be outside regardless of actual distance
  // assuming the voxel grid has padding around the mesh.
  // all threads hanging from the faces are also labeld as outside quickly.
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      if (y == 0 || y == size[1] - 1 || z == 0 || z == size[2] - 1) {
        for (unsigned x = 0; x < size[0]; x++) {
          label(x, y, z) = 1;
        }
        continue;
      }
      label(0, y, z) = 1;
      label(size[0] - 1, y, z) = 1;
      for (unsigned x = 1; x < size[0] - 1; x++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned x = size[0] - 1; x > 0; x--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  for (unsigned x = 0; x < size[0]; x++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned z = 1; z < size[2] - 1; z++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned z = size[2] - 1; z > 0; z--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned x = 0; x < size[0]; x++) {
      for (unsigned y = 1; y < size[1] - 1; y++) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
      for (unsigned y = size[1] - 1; y > 0; y--) {
        short d = dist(x, y, z);
        if (d <= distThresh) {
          break;
        }
        label(x, y, z) = 1;
      }
    }
  }

  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{-1, 0, 0}, {0, -1, 0}, {0, 0, -1}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  for (unsigned z = 1; z < size[2] - 1; z++) {
    for (unsigned y = 1; y < size[1] - 1; y++) {
      for (unsigned x = 1; x < size[0] - 1; x++) {
        for (unsigned ni = 0; ni < NUM_NBR; ni++) {
          unsigned ux = unsigned(x + nbrOffset[ni][0]);
          unsigned uy = unsigned(y + nbrOffset[ni][1]);
          unsigned uz = unsigned(z + nbrOffset[ni][2]);

          if (!label(x, y, z)) {
            continue;
          }
          uint8_t nbrLabel = label(ux, uy, uz);
          if (nbrLabel == 0) {
            uint16_t nbrDist = dist(ux, uy, uz);
            if (nbrDist >= distThresh) {
              FloodOutsideSeed(Vec3u(x, y, z), dist, distThresh, label);
            }
          }
        }
      }
    }
  }
  return label;
}

void FloodOutsideSeed8u(Vec3u seed, Array3D8u &vox, uint8_t outsideVal) {
  Vec3u size = vox.GetSize();
  size_t linearSeed = LinearIdx(seed[0], seed[1], seed[2], size);
  std::deque<size_t> q(1, linearSeed);
  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
  while (!q.empty()) {
    size_t linearIdx = q.front();
    q.pop_front();
    Vec3u idx = GridIdx(linearIdx, size);
    vox(idx[0], idx[1], idx[2]) = outsideVal;
    for (unsigned ni = 0; ni < NUM_NBR; ni++) {
      int nx = int(idx[0] + nbrOffset[ni][0]);
      int ny = int(idx[1] + nbrOffset[ni][1]);
      int nz = int(idx[2] + nbrOffset[ni][2]);
      if (!InBound(nx, ny, nz, size)) {
        continue;
      }
      unsigned ux = unsigned(nx), uy = unsigned(ny), uz = unsigned(nz);
      uint8_t nbrLabel = vox(ux, uy, uz);
      if (nbrLabel == 0) {
        vox(ux, uy, uz) = outsideVal;
        size_t nbrLinear = LinearIdx(ux, uy, uz, size);
        q.push_back(nbrLinear);
      }
    }
  }
}

void FloodOutside8u(Array3D8u &vox, uint8_t insideVal) {
  Vec3u size = vox.GetSize();
  // temporary outside value.
  // in the end outside are set to 0.
  uint8_t outsideVal = insideVal + 1;
  if (outsideVal == 0) {
    outsideVal++;
  }
  // all rows hanging from 6 voxel grid walls are labeled as outside
  // voxels in empty regions could be set to outsideVal multiple times redundantly.
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0] - 1; x++) {
        auto val = vox(x, y, z);
        if (val > 0) {
          break;
        }
        vox(x, y, z) = outsideVal;
      }

      for (unsigned x = size[0] - 1; x > 0; x--) {
        auto val = vox(x, y, z);
        if (val > 0) {
          break;
        }
        vox(x, y, z) = outsideVal;
      }
    }
  }

  for (unsigned x = 0; x < size[0]; x++) {
    for (unsigned y = 0; y < size[1]; y++) {      
      for (unsigned z = 0; z < size[2] - 1; z++) {
        auto val = vox(x, y, z);
        if (val > 0 && val != outsideVal) {
          break;
        }
        vox(x, y, z) = outsideVal;
      }
      
      for (unsigned z = size[2] - 1; z > 0; z--) {
        auto val = vox(x, y, z);
        if (val > 0 && val != outsideVal) {
          break;
        }
        vox(x, y, z) = outsideVal;
      }
    }
  }

  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned x = 0; x < size[0]; x++) {
      for (unsigned y = 0; y < size[1] - 1; y++) {
        auto val = vox(x, y, z);
        if (val > 0 && val != outsideVal) {
          break;
        }
        vox(x, y, z) = outsideVal;
      }
      for (unsigned y = size[1] - 1; y > 0; y--) {
        auto val = vox(x, y, z);
        if (val > 0 && val != outsideVal) {
          break;
        }
        vox(x, y, z) = outsideVal;
      }
    }
  }

  const unsigned NUM_NBR = 6;
  const int nbrOffset[6][3] = {{-1, 0, 0}, {0, -1, 0}, {0, 0, -1}, {1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  for (unsigned z = 1; z < size[2] - 1; z++) {
    for (unsigned y = 1; y < size[1] - 1; y++) {
      for (unsigned x = 1; x < size[0] - 1; x++) {
        for (unsigned ni = 0; ni < NUM_NBR; ni++) {
          unsigned ux = unsigned(x + nbrOffset[ni][0]);
          unsigned uy = unsigned(y + nbrOffset[ni][1]);
          unsigned uz = unsigned(z + nbrOffset[ni][2]);
          // flood neighbors of outside voxels
          if (vox(x, y, z) != outsideVal) {
            continue;
          }
          uint8_t nbrLabel = vox(ux, uy, uz);
          if (nbrLabel == 0) {            
            FloodOutsideSeed8u(Vec3u(x, y, z), vox, outsideVal);
          }
        }
      }
    }
  }
  for (auto &val : vox.GetData()) {
    if (val == outsideVal) {
      val = 0;
    } else {
      val = insideVal;
    }
  }
}
