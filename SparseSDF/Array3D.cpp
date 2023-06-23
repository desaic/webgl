#include "Array3D.h"
#include <iostream>
#include <fstream>
#include <string>
Array3D::Array3D():pixelBytes(1)
{
}

Array3D::~Array3D()
{}

void Array3D::allocate(unsigned nx, unsigned ny, unsigned nz)
{
    size[0] = nx;
    size[1] = ny;
    size[2] = nz;
    size_t nVox = nx * ny* size_t(nz);
    data.resize(nVox);
}

void Array3D::Fill(uint8_t val) {
  for (size_t i = 0; i < data.size(); i++) {
    data[i] = val;
  }
}

void Array3D::loadFile(const std::string & filename)
{
  std::ifstream in(filename);
  if (!in.good()) {
    std::cout << "can not open " << filename << "\n";
    return;
  }
  in.read((char*)(&size), sizeof(size));
  in.read((char*)(&pixelBytes), sizeof(pixelBytes));
  size_t nPixels = size[0] * size[1] * size[2];
  size_t nBytes = nPixels * pixelBytes;
  data.resize(nBytes);
  in.read((char*)data.data(), nBytes);
  in.close();
}

void Array3D::saveFile(const std::string & filename)
{
  std::ofstream out(filename);
  if (!out.good()) {
    std::cout << "can not open " << filename << "\n";
    return;
  }
  out.write((char*)(&size), sizeof(size));
  out.write((char*)(&pixelBytes), sizeof(pixelBytes));
  out.write((char*)data.data(), data.size());
  out.close();
}

void flipY(Array3D& vol)
{
  for (unsigned x = 0; x < vol.size[0]; x++) {
    for (unsigned y = 0; y < vol.size[1] / 2; y++) {
      for (unsigned z = 0; z < vol.size[2]; z++) {
        uint8_t tmp = vol(x, y, z);
        vol(x, y, z) = vol(x, vol.size[1] - y - 1, z);
        vol(x, vol.size[1] - y - 1, z) = tmp;
      }
    }
  }
}
