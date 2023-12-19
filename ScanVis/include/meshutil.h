#ifndef MESHUTIL_H
#define MESHUTIL_H

#include <string>
#include <vector>
#include <array>
#include "Array3D.h"

//save calibration points to obj file for debugging.
//length of table should be tsize[0] * tsize[1] * tsize[2] * 3.
void saveCalibObj(const float * table, const int * tsize,
                  std::string filename, int subsample = 10);

//save list of 3d points to .obj mesh file.
void savePointsObj(const std::string & fileName, const std::vector<std::array<float, 3 > > & points);

///save the boundary of a volume into obj file 
/// closed by rectangles.
/// @param voxRes 3 floats for voxel resolution in x y z 
void SaveVolAsObjMesh(std::string outfile, const Array3D8u & vol,
    float * voxRes, int mat);

#endif // MESHUTIL_H
