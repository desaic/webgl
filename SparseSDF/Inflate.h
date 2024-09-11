#pragma once

#include "Array3D.h"
#include "TrigMesh.h"

class AdapDF;
struct InflateConf {
  float thicknessMM = 1.0f;
  float voxResMM = 0.5f;
  unsigned MAX_GRID_SIZE = 1000;
};

void ComputeCoarseDist(AdapDF* df);
float ComputeVoxelSize(const TrigMesh& mesh, float voxResMM,
                       unsigned MAX_GRID_SIZE);
TrigMesh InflateMesh(const InflateConf& conf, TrigMesh& mesh);
Array3D8u FloodOutside(const Array3D<short>& dist, float distThresh);