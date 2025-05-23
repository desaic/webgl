#pragma once
#include "Array3d.h"
#include <string>

void SaveVoxTxt(const Array3D8u& grid, float voxRes, const std::string& filename,
  const Vec3f origin = {});

void SaveVoxTxt(const Array3Df& grid, Vec3f voxRes, const std::string& filename,
  const Vec3f origin = {});