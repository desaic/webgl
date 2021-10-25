#ifndef SLICE_TO_MESH_H
#define SLICE_TO_MESH_H

#include "Array3D.h"
#include "ConfigFile.hpp"

#include <string>

void LoadSlices(std::string dir,
  Array3D8u&vol, Vec3u mn, Vec3u mx);

void slices2Meshes(const ConfigFile& conf);

#endif