#ifndef MESHUTIL_H
#define MESHUTIL_H

#include <array>
#include <string>
#include <vector>

#include "Array2D.h"
#include "Array3D.h"
#include "SurfacePoint.h"
#include "TrigMesh.h"

// save list of 3d points to .obj mesh file.
void savePointsObj(const std::string& fileName, const std::vector<std::array<float, 3> >& points);

void SaveVolAsObjMesh(std::string outfile, const Array3D8u& vol, float* voxRes,
                 int mat);

  TrigMesh MarchCubes(std::vector<uint8_t>& vol, std::array<unsigned, 3>& gsize);

void MergeCloseVertices(TrigMesh& m);

void SamplePointsOneTrig(unsigned tIdx, const TrigMesh& m, std::vector<SurfacePoint>& points,
                         float spacing);

void SamplePoints(const TrigMesh& m, std::vector<SurfacePoint>& points, float spacing);

float SurfaceArea(const TrigMesh& m);

float UVArea(const TrigMesh& m);

#endif  // MESHUTIL_H