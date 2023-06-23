#ifndef MESHUTIL_H
#define MESHUTIL_H

#include <string>
#include <vector>
#include <array>
#include "Array3D.h"
#include "TrigMesh.h"

//save calibration points to obj file for debugging.
//length of table should be tsize[0] * tsize[1] * tsize[2] * 3.
void saveCalibObj(const float * table, const int * tsize,
                  std::string filename, int subsample = 10);

//save list of 3d points to .obj mesh file.
void savePointsObj(const std::string & fileName, const std::vector<std::array<float, 3 > > & points);

void SaveVolAsObjMesh(std::string outfile, const Array3D8u& vol, float* voxRes,
                      float* origin, int mat);

TrigMesh MarchCubes(std::vector<uint8_t>& vol, std::array<unsigned, 3>& gsize);

#endif  // MESHUTIL_H