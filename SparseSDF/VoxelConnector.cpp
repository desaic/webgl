#include "Array2D.h"
#include "Array3D.h"
#include "cpu_voxelizer.h"
#include "BBox.h"
#include "meshutil.h"
#include "Stopwatch.h"
#include "SimpleVoxCb.h"
#include "TrigMesh.h"
#include "Vec2.h"

#include <iostream>
#include <string>
#include <fstream>

void PrintVol(const Array3D8u& arr, std::string& filename) {
  std::ofstream out(filename);
  Vec3u size = arr.GetSize();
  out << size[1] << " " << size[0] << " " << size[2] << "\n";
  for (unsigned z = 0; z < size[2]; z++) {
    for (unsigned x = 0; x < size[0]; x++) {
      for (unsigned y = 0; y < size[1]; y++) {
        out << int(arr(x, y, z)) << " ";
      }
      out << "\n";
    }
  }
}

void AddToQueue(unsigned x, unsigned y, Array2D8u& label,
                std::vector<Vec2u>& q) {
  Vec2u size = label.GetSize();

  if (x >= size[0] || y >= size[1]) {
    return;
  }

  if (label(x, y) > 0) {
    return;
  }
  label(x, y) = 1;
  q.push_back(Vec2u(x, y));
}

// does not work for internal cavities.
void FloodFillSlices(Array3D8u& arr, uint8_t id) {
  Vec3u size = arr.GetSize();
  for (unsigned z = 0; z < size[2]; z++) {
    Array2D8u label(size[0], size[1]);
    // 0 for unvisited, 1 for queued, 2 for outside, 3 for inside
    label.Fill(0);
    // flood voxels reachable from outside
    std::vector<Vec2u> q;

    for (unsigned y = 0; y < size[1]; y++) {
      uint8_t mat = arr(0, y, z);
      if (mat == id) {
        label(0, y) = 3;
      } else {
        q.push_back(Vec2u(0, y));
        label(0, y) = 2;
      }

      mat = arr(size[0] - 1, y, z);
      if (mat == id) {
        label(size[0] - 1, y) = 3;
      } else {
        q.push_back(Vec2u(size[0] - 1, y));
        label(size[0] - 1, y) = 2;
      }
    }
    for (unsigned x = 0; x < size[0]; x++) {
      uint8_t mat = arr(x, 0, z);
      if (mat == id) {
        label(x, 0) = 3;
      } else {
        q.push_back(Vec2u(x, 0));
        label(x, 0) = 2;
      }

      mat = arr(x, size[1] - 1, z);
      if (mat == id) {
        label(x, size[1] - 1) = 3;
      } else {
        q.push_back(Vec2u(x, size[1] - 1));
        label(x, size[1] - 1) = 2;
      }
    }

    while (!q.empty()) {
      Vec2u coord = q.back();
      q.pop_back();
      uint8_t mat = arr(coord[0], coord[1], z);
      if (mat != id) {
        label(coord[0], coord[1]) = 2;
      } else {
        label(coord[0], coord[1]) = 3;
        continue;
      }
      AddToQueue(coord[0] - 1, coord[1], label, q);
      AddToQueue(coord[0] + 1, coord[1], label, q);
      AddToQueue(coord[0], coord[1] - 1, label, q);
      AddToQueue(coord[0], coord[1] + 1, label, q);
    }
    for (unsigned y = 0; y < size[1]; y++) {
      for (unsigned x = 0; x < size[0]; x++) {
        if (label(x, y) != 2) {
          arr(x, y, z) = id;
        }
      }
    }
  }
}

void VoxelConnector() {
  // std::string fileName = "F:/dolphin/meshes/fish/salmon.stl";
  // std::string fileName = "F:/dolphin/meshes/lattice_big/MeshLattice.stl";
  std::string fileName1 = "F:/dolphin/meshes/shoeInsole/conn/Hook80umWax.STL";
  std::string fileName2 = "F:/dolphin/meshes/shoeInsole/conn/HookInternal.STL";
  TrigMesh mesh1;
  mesh1.LoadStl(fileName1);
  TrigMesh mesh2;
  mesh2.LoadStl(fileName2);

  Array3D8u grid;
  voxconf conf;

  BBox box;
  ComputeBBox(mesh1.v, box);

  conf.unit = Vec3f(0.032, 0.032, 0.032);

  // add margin for narrow band sdf
  // sdf.band = std::min(sdf.band, AdapSDF::MAX_BAND);
  // box.vmin = box.vmin - float(sdf.band) * conf.unit;
  // box.vmax = box.vmax + float(sdf.band) * conf.unit;
  box.vmin[0] -= 0.3;
  box.vmax[0] += 0.3;
  conf.origin = box.vmin;

  Vec3f count = (box.vmax - box.vmin) / conf.unit;
  conf.gridSize = Vec3u(count[0] + 1, count[1] + 1, count[2] + 1);

  count[0]++;
  count[1]++;
  count[2]++;
  Utils::Stopwatch timer;
  timer.Start();
  SimpleVoxCb cb;
  grid.Allocate(conf.gridSize, 0);
  cb.grid = &grid;

  cpu_voxelize_mesh(conf, &mesh1, cb);
  FloodFillSlices(grid, 1);
  cb.matId = 2;

  cpu_voxelize_mesh(conf, &mesh2, cb);
  FloodFillSlices(grid, 2);
  float ms = timer.ElapsedMS();
  std::cout << "vox time " << ms << "\n";
  SaveVolAsObjMesh("voxels.obj", grid, (float*)(&conf.unit),
                   (float*)(&box.vmin), 1);
  std::string txtFile = "hookGrid.txt";
  PrintVol(grid, txtFile);
}
