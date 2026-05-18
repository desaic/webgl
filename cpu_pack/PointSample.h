#pragma once

#include "TrigMesh.h"
#include "Vec3.h"

#include <string>
#include <vector>

class SamplePoint {
  public:
    Vec3f x;
    Vec3f n;
    SamplePoint (Vec3f pos, Vec3f normal):x(pos), n(normal){}
};

/// @return -1 mesh missing vertex normals. 0 on success.
int SamplePoints (const TrigMesh & mesh, float eps, std::vector<SamplePoint> & points);

void SavePointsObj(const std::string & filename, const std::vector<SamplePoint> & points);
void TestSamplePoints();