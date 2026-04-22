#include "VoxIO.h"
#include <fstream>
#include <iostream>
#include <iomanip>

void SaveVoxTxt(const Array3D8u& grid, float voxRes, const std::string& filename, const Vec3f origin) {
  std::ofstream out(filename);
  if (!out.good()) {
    std::cout << "cannot open output " << filename << "\n";
    return;
  }
  out << "voxelSize\n" << voxRes << " " << voxRes << " " << voxRes << "\n";
  out << "spacing\n0.2\ndirection\n0 0 1\n";
  out << "origin\n" << origin[0] << " " << origin[1] << " " << origin[2] << "\n";
  out << "grid\n";
  Vec3u gridsize = grid.GetSize();
  out << gridsize[0] << " " << gridsize[1] << " " << gridsize[2] << "\n";
  for (unsigned z = 0; z < gridsize[2]; z++) {
    for (unsigned y = 0; y < gridsize[1]; y++) {
      for (unsigned x = 0; x < gridsize[0]; x++) {
        out << int(grid(x, y, z)) << " ";
      }
      out << "\n";
    }
  }
  out << "\n";
}

void SaveVoxTxt(const Array3Df& grid, Vec3f voxRes, const std::string& filename, const Vec3f origin) {
  std::ofstream out(filename);
  if (!out.good()) {
    std::cout << "cannot open output " << filename << "\n";
    return;
  }
  out << "origin\n" << origin[0] << " " << origin[1] << " " << origin[2] << "\n";
  out << "voxelSize\n" << voxRes[0] << " " << voxRes[1] << " " << voxRes[2] << "\n";  
  out << "grid\n";
  Vec3u gridsize = grid.GetSize();
  out << gridsize[0] << " " << gridsize[1] << " " << gridsize[2] << "\n";
  out << std::setprecision(3);
  for (unsigned z = 0; z < gridsize[2]; z++) {
    for (unsigned y = 0; y < gridsize[1]; y++) {
      for (unsigned x = 0; x < gridsize[0]; x++) {
        out << grid(x, y, z) << " ";
      }
      out << "\n";
    }
  }
  out << "\n";
}
